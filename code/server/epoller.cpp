#include "epoller.h"

Epoller::Epoller(int max_events) {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    events_.resize(max_events);
    assert(epoll_fd_ >= 0 and events_.size() > 0);   
}

Epoller::~Epoller() {
    close(epoll_fd_);
}

bool Epoller::add_fd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}   

bool Epoller::mod_fd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoller::del_fd(int fd) {
    if(fd < 0) return false;
    return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int Epoller::wait(int timeoutMs) {
    return epoll_wait(epoll_fd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::get_event_fd(size_t i) const {
    assert(i < events_.size());
    return events_[i].data.fd;
}

uint32_t Epoller::get_events(size_t i) const {
    assert(i < events_.size());
    return events_[i].events;
}

