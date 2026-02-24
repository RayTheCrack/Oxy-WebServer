#include "sqlconnpool.h"

SqlConnPool::SqlConnPool() {
    useCount_.store(0);
    freeCount_.store(0);
}

SqlConnPool& SqlConnPool::getInstance() {
    static SqlConnPool connPool;
    return connPool;
}

void SqlConnPool::Init(const char* host, int port,
    const char* user, const char* pwd, const char* dbName
    , int connSize) {
    assert(connSize > 0);
    int created = 0;
    for(int i=1;i<=connSize;i++) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if(!sql) {
            LOG_ERROR("mysql_init error");
            continue;
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if(!sql) {
            LOG_ERROR("mysql_real_connect error");
            mysql_close(sql);
            continue;
        }
        if(mysql_set_character_set(sql, "utf8mb4") != 0) {
            LOG_WARN(
                "SqlConnPool Init: set charset utf8mb4 failed (index: {}) | err: %s",
                i, mysql_error(sql)
            );
            // 字符集设置失败不影响连接使用，仅打印警告
        }
        created++;
        connQue_.push(sql); // 将连接放入队列
    }
    MAX_CONN_ = created; // 设置最大连接数
    freeCount_.store(created); // 设置空闲连接数
    if(created == 0) {
        LOG_WARN("SqlConnPool Init: No database connections created. Server will continue without database support.");
    } else {
        LOG_INFO(
            "SqlConnPool Init success | total conn: {}, created: {}, sem init: {}",
            MAX_CONN_, created, created);
    }
}
// 获取连接
MYSQL* SqlConnPool::getConnection() {
    MYSQL* sql = nullptr;
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this](){return freeCount_ > 0 and !connQue_.empty();});
    assert(!connQue_.empty());
    sql = connQue_.front();
    connQue_.pop();
    freeCount_.fetch_sub(1); // 空闲连接数减1
    useCount_.fetch_add(1); // 使用连接数加1
    LOG_DEBUG("Get sql connection success! free: {}, use: {}", freeCount_.load(), useCount_.load());
    return sql; // 返回连接
}

// 释放连接进入连接池复用
void SqlConnPool::freeConnection(MYSQL* sql) {
    assert(sql);
    std::unique_lock<std::mutex> lock(mtx_);
    connQue_.push(sql);
    freeCount_.fetch_add(1);
    useCount_.fetch_sub(1); 
    cv_.notify_one(); // 唤醒一个等待的线程
    LOG_DEBUG("Free sql connection success! free: {}, use: {}", freeCount_.load(), useCount_.load());
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(mtx_);
    while(!connQue_.empty()) {
        auto sql = connQue_.front();
        connQue_.pop();
        mysql_close(sql);
    }
    LOG_INFO("SqlConnPool Close success! total conn: {}", MAX_CONN_);
    mysql_library_end(); // 关闭mysql库
}

int SqlConnPool::GetFreeConnCnt() const {
    return freeCount_.load();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
