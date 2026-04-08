#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 
    资源在对象构造初始化 资源在对象析构时释放
    
    为什么不使用RAII容器（如unique_ptr）管理MYSQL的指针？
    
    MYSQL* 是 MySQL C API 提供的C 语言指针，不支持 C++ 析构函数，必须手动调用 mysql_close()。
    连接池的核心是连接复用，而非自动销毁，unique_ptr 的自动释放机制反而不适用。
    裸指针在这里是设计选择，不是缺陷，更符合高性能 WebServer 的需求。
*/

class SqlConnRAII {
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool) {
        assert(connpool);
        *sql = connpool->getConnection();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_->freeConnection(sql_); }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H
