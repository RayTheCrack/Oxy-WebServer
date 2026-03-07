#include "heaptimer.h"

HeapTimer::HeapTimer() {
    heap_.reserve(64); // 预留64个空间
}

HeapTimer::~HeapTimer() {
    clear();
}

static size_t get_ms_timestamp() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void HeapTimer::shift_up_(size_t curnode) {
    if(curnode >= heap_.size()) {
        return;
    }
    size_t father = (curnode - 1) / 2;
    while(true) { // 调整至不能再上浮
        if(heap_[curnode].first >= heap_[father].first) break;
        swap_node_(curnode, father); // 交换节点
        curnode = father;
        if(curnode == 0) break;
        father = (curnode - 1) / 2;
    }
}

void HeapTimer::shift_down_(size_t curnode) {
    if(curnode >= heap_.size()) {
        return;
    }
    size_t siz = heap_.size();
    size_t left_child = (curnode << 1) | 1;
    while(left_child < siz) {
        size_t min_child = left_child;
        size_t right_child = left_child + 1;
        if(right_child < siz and heap_[right_child].first < heap_[left_child].first)
            min_child = right_child;
        // 当前节点更小 则已满足最小堆
        if(heap_[curnode].first <= heap_[min_child].first)
            break;
        swap_node_(curnode, min_child);
        // 交换后继续向下遍历
        curnode = min_child;
        left_child = (curnode << 1) | 1;
    }
} 

void HeapTimer::swap_node_(size_t node_x, size_t node_y) {
    if(node_x >= heap_.size() || node_y >= heap_.size()) {
        return;
    }
    std::swap(heap_[node_x], heap_[node_y]);
    map_[heap_[node_x].second] = node_x;
    map_[heap_[node_y].second] = node_y;
}

void HeapTimer::add(int fd, int timeout, const TimeoutCB& cb) {
    assert(fd >= 0 && timeout >= 0);
    size_t i;
    if(map_.count(fd) == 0) { // 堆尾插入 + 上浮
        i = heap_.size();
        heap_.emplace_back(get_ms_timestamp() + timeout, fd);
        map_[fd] = i;
        // 存储回调函数
        cb_map_[fd] = cb;
        shift_up_(i);
        LOG_DEBUG("HeapTimer add fd {}, expire time: {} ms", fd, heap_[i].first);
    } else { // 已有节点：更新过期时间 + 调整堆
        i = map_[fd];
        heap_[i].first = get_ms_timestamp() + timeout;
        // 更新回调函数（如果传入了新的cb）
        if(cb) cb_map_[fd] = cb;
        // 调整堆
        shift_up_(i);
        shift_down_(i);
        LOG_DEBUG("HeapTimer update fd {}, new expire time: {} ms", fd, heap_[i].first);
    }
}

// 调整指定fd的超时时间
void HeapTimer::adjust(int fd, int new_expire_time) {
    assert(fd >= 0 and new_expire_time >= 0 and map_.count(fd) > 0);
    size_t idx = map_[fd];
    // 更新过期时间（毫秒级）
    heap_[idx].first = get_ms_timestamp() + new_expire_time;
    // 调整堆
    shift_up_(idx);
    shift_down_(idx);
    LOG_DEBUG("HeapTimer adjust fd {}, new expire time: {} ms", fd, heap_[idx].first);
}

void HeapTimer::do_work(int fd) {
    assert(fd >= 0);
    if(heap_.empty() or map_.count(fd) == 0) return;
    // 获取节点索引
    size_t idx = map_[fd];
    // 先删除节点，避免在回调函数中再次调用do_work导致问题
    del_(idx);
    // 执行回调函数（如果存在）
    if(cb_map_.count(fd)) {
        cb_map_[fd]();
    }
    LOG_DEBUG("HeapTimer do_work fd {}", fd);
}

void HeapTimer::del_(size_t node) {
    if(heap_.empty() || node >= heap_.size()) {
        return;
    }
    size_t last_idx = heap_.size() - 1;
    // 如果是最后一个节点，直接删除
    if(node == last_idx) {
        int fd = heap_[node].second;
        map_.erase(fd);
        cb_map_.erase(fd);
        heap_.pop_back();
        LOG_DEBUG("HeapTimer del_ fd {}, index {}", fd, node);
        return;
    }
    // 交换目标节点和堆尾节点
    int del_fd = heap_[node].second;
    swap_node_(node, last_idx);
    // 删除堆尾（原目标节点）
    map_.erase(del_fd);
    cb_map_.erase(del_fd);
    heap_.pop_back();
    // 调整堆（上浮+下沉）, 此时node为原堆尾节点
    if(node < heap_.size()) {
        shift_up_(node);
        shift_down_(node);
    }
    LOG_DEBUG("HeapTimer del_ fd {}, index {}", del_fd, node);
}

void HeapTimer::tick() {
    if(heap_.empty()) return;
    size_t now = get_ms_timestamp();
    // 循环检查堆顶（最小堆：堆顶是最早过期的）
    while(!heap_.empty()) {
        Node top = heap_[0];
        // 堆顶未过期，直接退出（后续节点更晚过期）
        if(top.first > now) break;
        // 堆顶过期：执行回调
        do_work(top.second);
        // 删除堆顶节点
        pop();
    }
}

void HeapTimer::pop() {
    if(heap_.empty()) return;
    del_(0);
}

int HeapTimer::get_next_tick() {
    if(heap_.empty()) return -1; // 无定时器：epoll_wait永久阻塞
    size_t now = get_ms_timestamp();
    size_t expire = heap_[0].first;
    // 计算剩余时间（避免负数）
    int next_tick = expire > now ? static_cast<int>(expire - now) : 0;
    LOG_DEBUG("HeapTimer next tick: {} ms", next_tick);
    return next_tick;
}

void HeapTimer::clear() {
    // 清空映射和回调
    map_.clear();
    cb_map_.clear();
    // 清空堆
    heap_.clear();
    LOG_DEBUG("HeapTimer clear all timers");
}
