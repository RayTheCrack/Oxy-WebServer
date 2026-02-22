#include "httpconnect.h"

std::atomic<int> HttpConnect::user_count(0);
bool HttpConnect::is_edge_trigger;
const char* HttpConnect::srcDir = "";

HttpConnect::HttpConnect() {
    fd_ = -1;
    addr_ = {0};
    is_running_.store(false);
    isKeepAlive_ = false;
    iovcnt_ = 0;
    iov[0] = iov[1] = {0}; // 初始化iovec
}

HttpConnect::~HttpConnect() {
    close(); // 关闭连接
}

void HttpConnect::init(int socket_fd, const sockaddr_in& addr) {
    assert(socket_fd > 0);
    user_count.fetch_add(1); // 增加用户数
    fd_ = socket_fd;
    addr_ = addr;
    is_running_.store(true); // 设置为运行状态
    readBuff_.reset(); // 重置缓冲区
    writeBuff_.reset(); 
    LOG_INFO("Client[{}]({}:{}) init, user_count={}", fd_, get_ip(), get_port(), user_count.load());
}

void HttpConnect::close() {
    response_.UnmapFile();
    if(is_running_.load()) {
        is_running_.store(false); // 设置为非运行状态
        user_count.fetch_sub(1); // 减少用户数
        ::close(fd_); // 关闭连接
        LOG_INFO("Client[{}]({}:{}) close, user_count={}", fd_, get_ip(), get_port(), user_count.load());
    }
}

int HttpConnect::get_fd() const {
    return fd_;
}

sockaddr_in HttpConnect::get_addr() const {
    return addr_;
}

const char* HttpConnect::get_ip() const {
    return inet_ntoa(addr_.sin_addr);
}

int16_t HttpConnect::get_port() const {
    return ntohs(addr_.sin_port);
}

ssize_t HttpConnect::read(int fd, Buffer& buff) {
    if(fd < 0 or !is_running_) {
        LOG_ERROR("read failed: invalid fd or connection closed");
        return -1;
    }
    ssize_t read_bytes = 0;
    // 边缘触发（ET）模式下，一次性读尽socket中的数据
    while(true) 
    {
        char temp_buff[65535] = {0}; // 临时缓冲区（64K）
        iov[0] = {buff.begin_write(), buff.writable_size()};
        iov[1] = {temp_buff, sizeof(temp_buff)}; 
        ssize_t len = readv(fd, iov, 2);
        if(len < 0) 
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) // 无数据可读
                break;
            // 真正的错误（如连接断开、系统错误）
            LOG_ERROR("readv failed, fd: {}, errno: {}, info: {}", 
                     fd, errno, strerror(errno));
            read_bytes = -1;
            break;
        } 
        else if(len == 0) 
        {
            // len=0表示客户端主动关闭连接（FIN包）
            LOG_INFO("Client[{}] closed connection", fd);
            read_bytes = 0;
            break;
        }
        // 取到数据，写入目标缓冲区
        if(len <= iov[0].iov_len) buff.append(static_cast<char*>(iov[0].iov_base), len);
        else {
            buff.append(static_cast<char*>(iov[0].iov_base), iov[0].iov_len);
            buff.append(temp_buff, len - iov[0].iov_len);
        }
        read_bytes += len;
        // 水平触发（LT）模式下，无需读尽，读一次即可退出
        if (!is_edge_trigger) {
            break;
        }
    }
    return read_bytes;
}

ssize_t HttpConnect::write(int fd, Buffer& buff) {
    if (fd < 0 or !is_running_ or buff.readable_size() == 0) {
        return 0; // 无数据可写，返回0
    }
    ssize_t write_bytes = 0;
    size_t readable = buff.readable_size(); // 缓冲区中待写数据长度
    // 初始化iov结构体，仅用iov[0]
    iov[0].iov_base = const_cast<char*>(buff.peek()); 
    iov[0].iov_len = readable;
    // 边缘触发（ET）模式下，一次性写尽数据
    while(true) {
        // 使用writev发送数据（比多次write减少系统调用）
        ssize_t len = writev(fd, iov, 1);
        if(len < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满，暂时无法写入（ET模式下需记录未写完的数据）
                LOG_DEBUG("writev EAGAIN, fd: {}, remain bytes: {}", 
                          fd, readable - write_bytes);
                break;
            }
            // 真正的写错误
            LOG_ERROR("writev failed, fd: {}, errno: {}, info: {}", 
                     fd, errno, strerror(errno));
            close(); // 错误时关闭连接
            return -1;
        }
        // 更新已写字节数和缓冲区指针
        write_bytes += len;
        if(write_bytes >= readable) {
            // 缓冲区数据全部写完
            buff.reset(); // 清空缓冲区
            break;
        } else {
            // 只部分写入（非ET模式可能出现），移动读指针
            buff.retrieve(len);
            iov[0].iov_base = const_cast<char*>(buff.peek());
            iov[0].iov_len = buff.readable_size();
            readable = buff.readable_size();
            // 水平触发（LT）模式下，无需继续写
            if (!is_edge_trigger) {
                break;
            }
        }
    }
    // 处理连接关闭逻辑：短连接则写完数据后关闭
    if(write_bytes > 0 and !IsKeepAlive()) is_running_ = false; // 标记连接待关闭
    return write_bytes;
}

ssize_t HttpConnect::writeResponse(int fd) {
    if (fd < 0 or !is_running_) {
        return 0; // 无效fd或连接已关闭，返回0
    }
    ssize_t write_bytes = 0;
    size_t total_to_write = iov[0].iov_len + (iovcnt_ > 1 ? iov[1].iov_len : 0);
    if (total_to_write == 0) {
        return 0; // 无数据可写，返回0
    }
    // 边缘触发（ET）模式下，一次性写尽数据
    while(true) {
        // 使用writev发送数据（比多次write减少系统调用）
        ssize_t len = writev(fd, iov, iovcnt_);
        if(len < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // 写缓冲区满，暂时无法写入（ET模式下需记录未写完的数据）
                LOG_DEBUG("writev EAGAIN, fd: {}, remain bytes: {}",
                          fd, total_to_write - write_bytes);
                break;
            }
            // 真正的写错误
            LOG_ERROR("writev failed, fd: {}, errno: {}, info: {}",
                     fd, errno, strerror(errno));
            close(); // 错误时关闭连接
            return -1;
        }
        // 更新已写字节数
        write_bytes += len;
        if(write_bytes >= total_to_write) 
        {
            // 数据全部写完，清空写缓冲区
            writeBuff_.reset();
            iov[0].iov_len = 0;
            if(iovcnt_ > 1) 
                iov[1].iov_len = 0;
            break;
        } else {
            // 只部分写入，更新iov
            if(len >= iov[0].iov_len) {
                // iov[0]已写完，处理iov[1]
                size_t remaining = len - iov[0].iov_len;
                iov[0].iov_len = 0;
                if(iovcnt_ > 1) {
                    iov[1].iov_base = static_cast<char*>(iov[1].iov_base) + remaining;
                    iov[1].iov_len -= remaining;
                }
            } else {
                // iov[0]未写完
                iov[0].iov_base = static_cast<char*>(iov[0].iov_base) + len;
                iov[0].iov_len -= len;
            }
            total_to_write = iov[0].iov_len + (iovcnt_ > 1 ? iov[1].iov_len : 0);
            // 水平触发（LT）模式下，无需继续写
            if (!is_edge_trigger) {
                break;
            }
        }
    }
    // 处理连接关闭逻辑：短连接则写完数据后关闭
    if(write_bytes > 0 and !IsKeepAlive()) is_running_ = false; // 标记连接待关闭
    return write_bytes;
}

bool HttpConnect::process() {
    // 初始化请求对象
    request_.init();
    // 校验读缓冲区是否有数据
    if (readBuff_.readable_size() <= 0) {
        LOG_DEBUG("Client[{}] read buffer is empty, process failed", fd_);
        return false;
    }
    // 解析HTTP请求
    bool parse_success = request_.parse(readBuff_);
    int status_code = parse_success ? 200 : 400; // 解析成功=200，失败=400
    bool is_keep_alive = parse_success ? request_.IsKeepAlive() : false; // 失败则关闭连接
    isKeepAlive_ = is_keep_alive; // 保存Keep-Alive状态
    if(parse_success) 
    {
        LOG_DEBUG("Client[{}]({}:{}) request: {}", 
                 fd_, get_ip(), get_port(), request_.path().c_str());
    } 
    else {
        LOG_WARN("Client[{}]({}:{}) parse request failed, status: 400", 
                 fd_, get_ip(), get_port());
    }
    // 初始化响应对象
    // 解析失败时传入400，解析成功时传入-1让MakeResponse方法中的stat会被调用
    response_.Init(srcDir, request_.path(), is_keep_alive, parse_success ? -1 : 400);
    // 清空写缓冲区
    writeBuff_.reset();
    // 生成响应数据到写缓冲区
    response_.MakeResponse(writeBuff_);
    // 初始化iov数组（响应头+文件内容）
    // 响应头（writeBuff_中的数据）
    iov[0].iov_base = const_cast<char*>(writeBuff_.peek());
    iov[0].iov_len = writeBuff_.readable_size();
    iovcnt_ = 1; // 默认只有响应头
    // 如果有文件内容（如静态资源），添加到iov[1]
    if(response_.FileLen() > 0 and response_.GetFile() != nullptr) {
        iov[1].iov_base = response_.GetFile();
        iov[1].iov_len = response_.FileLen();
        iovcnt_ = 2;
        LOG_DEBUG("Client[{}] response with file, len: %{}, total iov: {}", 
                 fd_, response_.FileLen(), iovcnt_);
    }
    // 重置请求对象（释放资源）
    request_.init();
    return parse_success;
}