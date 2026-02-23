
## 【第 9 步】epoll 事件循环（Epoller）

### 目标
实现基于 epoll 的事件循环，处理文件描述符的读写事件。

## 【第 13 步】WebServer 主逻辑（WebServer）

### 目标
实现 WebServer 主逻辑，整合所有模块，处理客户端请求。

### 13.1 创建 `code/server/webserver.h`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>     // close()
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../epoller.h"
#include "../log/log.h"
#include "../heaptimer.h"
#include "../threadpool.h"
#include "../sqlconnpool.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const  char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    
    void Start();

private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);
    
    void DealListen_();
    void DealWrite_(HttpConn* client);
    void DealRead_(HttpConn* client);
    
    void SendError_(int fd, const char* info);
    void ExtentTime_(HttpConn* client);
    void CloseConn_(HttpConn* client);
    
    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess_(HttpConn* client);

    static const int MAX_FD = 65536;

    int port_;
    bool openLinger_;
    int timeoutMS_;  /* 毫秒 */
    bool isClose_;
    int listenFd_;
    char* srcDir_;
    
    uint32_t listenEvent_;
    uint32_t connEvent_;
    
    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConn> users_;
};

#endif
```

### 13.2 创建 `code/server/webserver.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "webserver.h"

WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger,
    int sqlPort, const char* sqlUser, const  char* sqlPwd,
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize):
    port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
    timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller()) {
    
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);
    
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    
    // 初始化日志系统
    if(openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                           (listenEvent_ & EPOLLET ? "ET": "LT"),
                           (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
    
    // 初始化数据库连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
    
    InitEventMode_(trigMode);
    if(!InitSocket_()) { isClose_ = true; }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode) {
    listenEvent_ = EPOLLRDHUP;
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    while(!isClose_) {
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                DealListen_();
            } else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);
            } else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]);
            } else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
}

void WebServer::DealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;}
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET);
}

void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int readErrno = 0;
    ssize_t n = client->read(&readErrno);
    if(n <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    OnProcess_(client);
}

void WebServer::OnProcess_(HttpConn* client) {
    if(client->process()) {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int writeErrno = 0;
    ssize_t n = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess_(client);
            return;
        }
    }
    else if(n < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { 
        timer_->adjust(client->GetFd(), timeoutMS_); 
    }
}

bool WebServer::InitSocket_() {
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
```

### 13.3 更新 Makefile

修改 `SOURCES` 行：
```makefile
SOURCES = $(SRC_DIR)/main.cpp config/config.cpp buffer/buffer.cpp log/log.cpp http/httprequest.cpp http/httpresponse.cpp http/httpconn.cpp server/epoller.cpp timer/heaptimer.cpp pool/threadpool.cpp pool/sqlconnpool.cpp server/webserver.cpp
```

### 13.4 更新 `code/main.cpp`

```cpp
/*
 * @Author       : mark
 * @Date         : 2024-02-12
 * @copyleft Apache 2.0
 */

#include "server/webserver.h"

int main(int argc, char *argv[]) {
    int port = 8080;
    int trigMode = 0;
    int timeoutMS = 60000;
    bool OptLinger = false;
    int sqlPort = 3306;
    const char* sqlUser = "root";
    const char* sqlPwd = "123456";
    const char* dbName = "webserver";
    int connPoolNum = 6;
    int threadNum = 8;
    bool openLog = true;
    int logLevel = 0;
    int logQueSize = 1024;

    // 解析命令行参数
    int opt;
    const char *str = "t:l:e:o:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            case 't': threadNum = atoi(optarg); break;
            case 'l': logQueSize = atoi(optarg); break;
            case 'e': trigMode = atoi(optarg); break;
            case 'o': openLog = atoi(optarg); break;
            default: break;
        }
    }

    // 创建WebServer实例并启动
    WebServer server(
        port, trigMode, timeoutMS, OptLinger,
        sqlPort, sqlUser, sqlPwd, dbName,
        connPoolNum, threadNum, openLog, logLevel, logQueSize
    );
    
    server.Start();
    
    return 0;
}
```

### 13.5 编译与测试

```bash
make clean
make
./bin/server
```

## 【第 14 步】静态资源与测试

### 目标
添加静态资源，并进行功能测试。

### 14.1 创建静态资源目录

```bash
mkdir -p resources/html
mkdir -p resources/image
mkdir -p resources/video
mkdir -p resources/js
mkdir -p resources/css
```

### 14.2 添加简单的 HTML 文件

创建 `resources/html/index.html`:

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>WebServer Test</title>
</head>
<body>
    <h1>Welcome to WebServer!</h1>
    <p>This is a test page for the WebServer.</p>
</body>
</html>
```

### 14.3 测试 WebServer

```bash
# 启动服务器
./bin/server

# 在另一个终端测试
curl http://localhost:8080/index.html
```

## 【第 15 步】性能测试

### 目标
使用 webbench 进行性能测试。

### 15.1 编译 webbench

```bash
cd webbench-1.5
make
```

### 15.2 运行性能测试

```bash
./webbench -c 100 -t 30 http://localhost:8080/index.html
```

## 【第 16 步】优化与文档

### 目标
优化代码性能，完善项目文档。

### 16.1 性能优化建议

1. 使用内存池减少内存分配开销
2. 优化日志系统，减少锁竞争
3. 使用零拷贝技术优化数据传输
4. 优化数据库查询，添加索引

### 16.2 完善文档

1. 添加详细的代码注释
2. 编写用户手册
3. 添加开发文档
4. 编写测试用例

## 总结

通过以上步骤，我们已经完成了一个基于 C++ 的 WebServer 项目，包括：

1. 项目骨架与构建系统
2. 配置模块（Config）
3. 缓冲区模块（Buffer）
4. 阻塞队列（BlockDeque）
5. 日志模块（Log）
6. HTTP 请求解析（HttpRequest）
7. HTTP 响应构造（HttpResponse）
8. HTTP 连接封装（HttpConn）
9. epoll 事件循环（Epoller）
10. 定时器模块（HeapTimer）
11. 线程池（ThreadPool）
12. 数据库连接池（SqlConnPool）
13. WebServer 主逻辑（WebServer）
14. 静态资源与测试
15. 性能测试
16. 优化与文档

这个项目实现了一个高性能的 Web 服务器，支持 HTTP 协议，具有异步日志、线程池、数据库连接池等功能，可以处理并发请求，并具有良好的扩展性和可维护性。
=======

>>>>>>> 431c097fcc60f48ce18d91356752377d48c137a6