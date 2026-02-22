#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <queue>
#include <time.h>
#include <unordered_map>
#include <functional>
#include <sys/time.h>
#include <assert.h>
#include <vector>
#include <chrono>
#include <arpa/inet.h>
#include <algorithm>

#include "../log/log.h"

typedef std::function<void()> TimeoutCB;
typedef std::pair<size_t, size_t> Node; // <expire_time, socket_fd>

class HeapTimer {

public:
    HeapTimer();
    ~HeapTimer();

    void adjust(int fd, int new_expire_time);

    void add(int fd, int time_out, const TimeoutCB& cb);

    void do_work(int fd);

    void tick();

    void clear();
    
    void pop();

    int get_next_tick(); // return next tick time
private:
    void del_(size_t node);

    void shift_up_(size_t curnode);

    void shift_down_(size_t curnode);

    void swap_node_(size_t node_x, size_t node_y);

    std::vector<Node> heap_;
    // socket_fd -> index in heap
    std::unordered_map<size_t, size_t> map_; 
    std::unordered_map<size_t, TimeoutCB> cb_map_;
};
#endif /* HEAPTIMER_H */
