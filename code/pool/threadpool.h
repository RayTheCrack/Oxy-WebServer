#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <atomic>
#include <thread>

#include "../log/log.h"

class ThreadPool {

public:
    explicit ThreadPool(size_t thread_cnt);

    ~ThreadPool() noexcept;

    template<typename F, typename... Args>
    void submit(F&& f, Args&&... args); // 提交任务

private:
    void NewThread(); // 创建新线程
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> is_running_; // 是否正在运行
};

template<typename F, typename... Args>
void ThreadPool::submit(F&& f, Args&&... args) {
    if(!is_running_) {
        LOG_ERROR("ThreadPool is not running, cannot submit task");
        return;
    }
    std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        tasks_queue_.emplace(std::move(task));
    }
    cv_.notify_one(); // 通知等待线程有新任务入队
}

#endif /* THREADPOOL_H */
