#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../http/httpconnect.h"
#include "../log/log.h"
#include "../pool/threadpool.h"
#include "../config/config.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnRAII.h"
#include "epoller.h"

class WebServer {

public:
    WebServer();

    ~WebServer();

    void start();
    void stop();
    
private:
    bool InitSocket_();
    void InitEventMode_(int trigMode);
    void AddClient_(int fd, sockaddr_in addr);

    void HandleListen_();
    void HandleWrite_(HttpConnect& client);
    void HandleRead_(HttpConnect& client);

    void SendError_(int fd, const char* info);
    void ExtentTime_(HttpConnect& client);
    void CloseConn_(HttpConnect& client);

    void OnRead_(HttpConnect& client);
    void OnWrite_(HttpConnect& client);
    void OnProcess(HttpConnect& client);

    static const int MAX_FD = 65536; // 最大文件描述符数量
    static int SetFdNonblock(int fd);

    int16_t port_;
    bool openLinger_;
    int timeoutMS_; // 客户端超时时间
    bool is_running_;
    int listenFd_;
    char* srcDir_;

    uint32_t listenEvent_;
    uint32_t connEvent_;

    std::unique_ptr<HeapTimer> timer_;
    std::unique_ptr<ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConnect> users_; // fd -> HttpConnect
};

#endif /* WEBSERVER_H */
