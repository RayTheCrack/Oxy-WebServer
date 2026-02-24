#include "webserver.h"
#include <iostream>
#include <string>
#include <cstring>

WebServer::WebServer()
{
    port_ = Config::getInstance().c_port;    
    timeoutMS_ = Config::getInstance().c_timeout;
    openLinger_ = Config::getInstance().c_isOptLinger;
    is_running_ = true;
    
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);

    timer_ = std::make_unique<HeapTimer>();
    threadpool_ = std::make_unique<ThreadPool>(Config::getInstance().c_thread_cnt);
    epoller_ = std::make_unique<Epoller>();

    HttpConnect::user_count.store(0);
    HttpConnect::srcDir = srcDir_;

    InitEventMode_(Config::getInstance().c_trigMode); // 初始化事件模式
    
    if(Config::getInstance().c_open_log) {
        LogLevel level;
        switch(Config::getInstance().c_log_level) {
            case 0: level = LogLevel::DEBUG; break;
            case 1: level = LogLevel::INFO; break;
            case 2: level = LogLevel::WARN; break;
            case 3: level = LogLevel::ERROR; break;
            default: level = LogLevel::INFO; break;
        }
        Logger::getInstance().initLogger(Config::getInstance().c_log_file.c_str(), level, Config::getInstance().c_log_queue_size);
        LOG_INFO("=============Oxy Server init success=============");
        LOG_INFO("port: {}, log_level: {}", port_, Config::getInstance().c_log_level);
        LOG_INFO("Resource root: {}, Open linger: {}", srcDir_, openLinger_ ? "true" : "false");
        LOG_INFO("Listen mode: {}, Connection mode: {}", (listenEvent_ & EPOLLET) ? "ET" : "LT", (connEvent_ & EPOLLET) ? "ET" : "LT");
        LOG_INFO("ThreadPool size: {}, SqlConnectPool size: {}", Config::getInstance().c_thread_cnt, Config::getInstance().c_conn_pool_num);
        LOG_INFO("http://localhost:{}/ now active for testing.", port_);
        LOG_INFO("=============Oxy Server init success=============");
    }

    SqlConnPool::getInstance().Init(Config::getInstance().c_db_host.c_str(), 
        Config::getInstance().c_db_port, Config::getInstance().c_db_user.c_str(), 
        Config::getInstance().c_db_password.c_str(), Config::getInstance().c_db_name.c_str(), Config::getInstance().c_conn_pool_num);
    
    if(!InitSocket_()) is_running_ = false; // 初始化socket失败则停止服务器
}

WebServer::~WebServer() {
    close(listenFd_);
    is_running_ = false;
    LOG_INFO("===============Server stopped================");
    free(srcDir_); // 释放资源路径内存
    SqlConnPool::getInstance().ClosePool(); // 关闭数据库连接池
    sleep(3); // 等待3s日志刷盘
    Logger::getInstance().shutdown(); // 关闭日志系统
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch(trigMode) {
        case 1: break; // 默认就是LT
        case 2: connEvent_ |= EPOLLET; break; // 连接使用ET
        case 3: listenEvent_ |= EPOLLET; break; // 监听socket使用ET
        case 4: listenEvent_ |= EPOLLET; connEvent_ |= EPOLLET; break; // 都使用ET
        default: break;
    }
    HttpConnect::is_edge_trigger = (connEvent_ & EPOLLET) != 0; // 更新HttpConnect的触发模式
}

void WebServer::stop() {
    is_running_ = false;
    LOG_INFO("Server stop signal received.");
}

void WebServer::start() {
    int timeMS = -1; // epoll wait timeout, -1 stands for non-block
    if(is_running_) LOG_INFO("=============Server start=============");
    while(is_running_) {
        if(timeoutMS_ > 0) timeMS = timer_->get_next_tick();
        int event_cnt = epoller_->wait(timeMS); // 获取就绪事件数量
        for(int i=0;i<event_cnt;++i) {
            int fd = epoller_->get_event_fd(i);
            uint32_t events = epoller_->get_events(i);
            if(fd == listenFd_) {
                HandleListen_();
            }
            // 优先处理异常事件，确保连接异常时能及时关闭，避免资源泄露
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0); // 确保fd在users_中存在
                CloseConn_(users_.at(fd));
            }
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                HandleRead_(users_.at(fd));
            }
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                HandleWrite_(users_.at(fd));
            }
            else {
                LOG_ERROR("Unexpected event!");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_ERROR("Send error to client[{}] error!", fd);
    }
    close(fd); 
}

void WebServer::CloseConn_(HttpConnect& client) {
    assert(client.get_fd() > 0);
    LOG_INFO("Client[{}] quit!", client.get_fd());
    epoller_->del_fd(client.get_fd());
    client.close(); // 关闭连接
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr); // 初始化Httpconnect
    // 将客户端 socket 设置为非阻塞，并加入 epoll 监听
    WebServer::SetFdNonblock(fd);
    if(!epoller_->add_fd(fd, connEvent_ | EPOLLIN)) {
        LOG_ERROR("Add client fd error: {}", fd);
        ::close(fd);
        return;
    }
    // 如果有超时时间，则加入定时器，回调函数为CloseConn_，参数为当前连接对象的引用
    if(timeoutMS_ > 0) timer_->add(fd, timeoutMS_, 
        std::bind(&WebServer::CloseConn_, this, std::ref(users_[fd])));
}

void WebServer::HandleListen_() {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, reinterpret_cast<sockaddr*>(&addr), &len);
        if(fd < 0) {
            LOG_ERROR("Accept error!");
            return;
        }
        else if(HttpConnect::user_count.load() >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients exceed max fd limit!");
            return;
        }
        AddClient_(fd, addr); // 添加新连接
    } while(listenEvent_ & EPOLLET); // 如果是ET模式，则循环，否则只执行一次
}

void WebServer::HandleRead_(HttpConnect& client) {
    assert(client.get_fd() > 0);
    ExtentTime_(client); // 延长连接超时时间
    threadpool_->submit(std::bind(&WebServer::OnRead_, this, std::ref(client)));
}

void WebServer::HandleWrite_(HttpConnect& client) {
    assert(client.get_fd() > 0);
    ExtentTime_(client); // 延长连接超时时间
    threadpool_->submit(std::bind(&WebServer::OnWrite_, this, std::ref(client)));
}

void WebServer::OnRead_(HttpConnect& client) {
    assert(client.get_fd() > 0);
    int read_errno = 0;
    ssize_t len = client.read(client.get_fd(), client.getReadBuffer(), read_errno);
    if(len <= 0 and read_errno != EAGAIN and read_errno != EWOULDBLOCK)
    {
        LOG_WARN("Client[{}] read error, errno: {}", client.get_fd(), read_errno);
        CloseConn_(client);
        return;
    }
    OnProcess(client); // 处理HTTP请求
}

void WebServer::OnProcess(HttpConnect& client) {
    if(client.process()) {
        epoller_->mod_fd(client.get_fd(), connEvent_ | EPOLLOUT); // 修改事件为可写
    }
    else {
        epoller_->mod_fd(client.get_fd(), connEvent_ | EPOLLIN); // 修改事件为可读，继续读取请求
    }
}

void WebServer::OnWrite_(HttpConnect& client) {
    assert(client.get_fd() > 0);
    // 使用 writeResponse 直接写入 iov 中的所有数据（响应头 + 文件内容）
    ssize_t len = client.writeResponse(client.get_fd());
    if(len < 0) {
        // writeResponse 已在错误时关闭连接
        CloseConn_(client);
        return;
    }
    if(client.to_write_bytes() == 0) {
        // 数据全部写完，准备下一次读取
        if(client.IsKeepAlive()) {
            OnProcess(client); // 继续处理请求，可能会有新的响应数据要写
            return;
        }
        CloseConn_(client); // 短连接，写完后关闭
        return;
    }
    // 仍有数据待写，继续等待 EPOLLOUT
    epoller_->mod_fd(client.get_fd(), connEvent_ | EPOLLOUT);
}

void WebServer::ExtentTime_(HttpConnect& client) {
    assert(client.get_fd() > 0);
    if(timeoutMS_ > 0) timer_->adjust(client.get_fd(), timeoutMS_); // 调整定时器
}

bool WebServer::InitSocket_() {
    int ret = 0;
    sockaddr_in addr;
    if((int)port_ > 65535 or (int)port_ < 1024) {
        LOG_ERROR("Port: {} error!", port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!");
        return false;
    }
    linger optlinger;
    memset(&optlinger, 0, sizeof(optlinger));
    if(openLinger_) {
        optlinger.l_onoff = 1;
        optlinger.l_linger = 1;
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optlinger, sizeof(optlinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Set socket options error!");
        return false;
    }
    int opt = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Set socket options error!");
        return false;
    }
    ret = bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Bind socket error!");
        return false;
    }
    ret = listen(listenFd_, 128);
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Listen socket error!");
        return false;
    }
    ret = epoller_->add_fd(listenFd_, listenEvent_ | EPOLLIN);
    if(!ret) {
        close(listenFd_);
        LOG_ERROR("Add listen fd error!");
        return false;
    }
    WebServer::SetFdNonblock(listenFd_);
    LOG_INFO("Server port: {}, listen mode: {}, Open linger: {}", port_, (listenEvent_ & EPOLLET) ? "ET" : "LT", openLinger_);      
    return true;
}

int WebServer::SetFdNonblock(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

