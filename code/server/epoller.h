#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int max_events = 1024);
    ~Epoller();

    bool add_fd(int fd, uint32_t events); // add fd to epoll
    bool mod_fd(int fd, uint32_t events); // modify events
    bool del_fd(int fd); // delete fd from epoll

    int wait(int timeout = -1);
    int get_event_fd(size_t i) const;
    uint32_t get_events(size_t i) const;
private:
    int epoll_fd_;
    std::vector<epoll_event> events_;
};

#endif /* EPOLLER_H */
