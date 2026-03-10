#ifndef HTTPCONNECT_H
#define HTTPCONNECT_H

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h> // readv/writev
#include <arpa/inet.h>  // sockaddr_int
#include <stdlib.h> // atoi();
#include <errno.h>
#include <atomic>

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConnect {

public: 
    HttpConnect();
    ~HttpConnect();

    void init(int socket_fd, const sockaddr_in& addr);

    // 每个连接的读写窗口
    ssize_t read(int fd, Buffer& buff, int& errno_);
    ssize_t write(int fd, Buffer& buff, int& errno_);
    // 兼容旧接口的便捷重载：测试代码可以不传 errno 参数
    inline ssize_t read(int fd, Buffer& buff) { int err=0; return read(fd, buff, err); }
    inline ssize_t write(int fd, Buffer& buff) { int err=0; return write(fd, buff, err); }

    ssize_t writeResponse(int fd); // 写入已准备好的响应数据

    void close();

    int get_fd() const;
    sockaddr_in get_addr() const;
    const char* get_ip() const;
    int16_t get_port() const;

    bool process(); // 处理HTTP请求
    // 获取待写入的总字节数（iov[0]+iov[1]）
    int to_write_bytes() { 
        return iov[0].iov_len + iov[1].iov_len;
    }

    bool IsKeepAlive() const {
        return isKeepAlive_;
    }

    Buffer& getReadBuffer() {
        return readBuff_;
    }

    Buffer& getWriteBuffer() {
        return writeBuff_;
    }

    static std::atomic<int> user_count; // 当前连接数
    static const char* srcDir; // 静态资源目录
    static bool is_edge_trigger; // 是否是边缘触发

private:
    int fd_; // socket fd

    sockaddr_in addr_; // client addr

    std::atomic<bool> is_running_;
    bool isKeepAlive_; // 是否保持连接

    int iovcnt_;
    iovec iov[2]; // readv/writev

    Buffer writeBuff_;
    Buffer readBuff_;

    HttpRequest request_;
    HttpResponse response_;
};

#endif /* HTTPCONNECT_H */
