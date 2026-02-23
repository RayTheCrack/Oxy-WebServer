#include "threadpool.h"

ThreadPool::ThreadPool(size_t thread_cnt) {
    is_running_.store(true);
    for(size_t i=0;i<thread_cnt;++i) {
        workers_.emplace_back(&ThreadPool::NewThread, this);
    }
}

ThreadPool::~ThreadPool() noexcept {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        is_running_.store(false);
    }
    cv_.notify_all();
    for(auto& t : workers_) {
        if(t.joinable()) t.join();
    }
}

void ThreadPool::NewThread(){
    while(true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this](){ return !tasks_queue_.empty() or !is_running_; });
        if(is_running_.load() == false and tasks_queue_.empty()) {
            return; // 线程池已停止且没有任务，退出线程
        }
        std::function<void()> task = std::move(tasks_queue_.front());
        tasks_queue_.pop();
        lock.unlock();
        task(); // 执行任务
    }
}