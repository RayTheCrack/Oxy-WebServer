#ifndef LOG_H
#define LOG_H

// 实现异步日志系统，支持日志级别、文件输出和日志滚动（按日期）。
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <thread>
#include <format>
#include <condition_variable>
#include <fstream>
#include <ctime>
#include <atomic>
#include <cstdarg>   
#include <cstdio>    
#include <unistd.h>  
#include <sys/stat.h>
#include <sys/time.h>

#include "blockqueue.h"
#include "../buffer/buffer.h"


enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

// 日志类 单例
// Log类是单例模式，负责日志的格式化、异步 / 同步写入、
// 日志文件轮转，核心逻辑是 “主线程生产日志→阻塞队列中转→刷盘线程消费日志”。

class Logger {
public:
    static Logger& getInstance();

    // 初始化日志系统，指定日志文件路径和日志级别
    void initLogger(const std::string& log_file,
         LogLevel level, int max_queue_size);
    // 记录日志
    void log(LogLevel level, const std::string& message);
    void log(LogLevel level, const Buffer& buffer);
    // 关闭日志系统，确保所有日志被写入文件
    void shutdown();
    // 设置日志级别
    void setLogLevel(LogLevel level);
    // 获取当前日志级别
    LogLevel getLogLevel() const;

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log_worker_thread(); // 刷盘线程函数，负责从队列中取日志并写入文件
    std::string get_timestamp(); // 获取当前时间戳，格式为 YYYY-MM-DD HH:MM:SS
    std::string get_level_name(LogLevel level);
    
    std::string log_file_; // 日志文件路径
    std::string current_date_; // 当前日志文件对应的日期，格式为 YYYY-MM-DD
    LogLevel current_level_; // 当前日志级别，只有级别高于等于这个级别的日志才会被记录
    std::ofstream log_stream_; // 日志文件流，用于写入日志文件

    std::unique_ptr<BlockDeque<std::string>> message_queue_; // 日志消息队列，异步写入时使用，注意是指针类型，只有在异步模式下才会初始化
    Buffer write_buffer_; // 写入缓冲区，用于批量写入日志
    std::thread worker_thread_; // 刷盘线程
    std::atomic<bool> is_running_; // 标志日志系统是否正在运行
    int64_t LEAST_FLUSH_SEC_GAP; // 最小刷新间隔，单位为秒
    std::chrono::time_point<std::chrono::steady_clock> last_flush_time_; // 上次刷新时间
    void flush(); // 将缓冲区中的日志写入文件
    void flush_if_need(); // 检查是否需要刷新日志文件
};

// C++20 std::format 实现的格式化字符串函数，支持任意类型参数，类似于 printf 风格, 占位符为 {}
template<typename... Args>
std::string format_string(const std::string& fmt, Args&&... args) {
    auto args_pack = std::make_format_args(std::forward<Args>(args)...);
    std::string msg = std::vformat(fmt, args_pack);
    return msg;
}

// 全局日志宏，方便使用
#define LOG_DEBUG(fmt, ...) \
    do { \
        Logger::getInstance().log(LogLevel::DEBUG, format_string(fmt, ##__VA_ARGS__)); \
    } while(0)
#define LOG_INFO(fmt, ...) \
    do { \
        Logger::getInstance().log(LogLevel::INFO, format_string(fmt, ##__VA_ARGS__)); \
    } while(0)
#define LOG_WARN(fmt, ...) \
    do { \
        Logger::getInstance().log(LogLevel::WARN, format_string(fmt, ##__VA_ARGS__)); \
    } while(0)
#define LOG_ERROR(fmt, ...) \
    do { \
        Logger::getInstance().log(LogLevel::ERROR, format_string(fmt, ##__VA_ARGS__)); \
    } while(0)  

#endif /* LOG_H */
