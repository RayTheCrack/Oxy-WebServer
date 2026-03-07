# OXYTHECRACK - 高性能 C++ Web 服务器

![C++](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus)
![Linux](https://img.shields.io/badge/Linux-Ubuntu-orange?logo=linux)
![MySQL](https://img.shields.io/badge/MySQL-8.0-blue?logo=mysql)
![License](https://img.shields.io/badge/License-MIT-green)

一个基于 C++20 标准开发的高性能、高并发的 HTTP Web 服务器，采用主从 Reactor 模型实现异步 I/O 处理。

## 🚀 核心特性

### 网络架构
- **I/O 多路复用**: 基于 Epoll 实现高效的异步 I/O 操作
- **主从 Reactor 模型**: 采用多线程线程池实现高并发处理，支持 LT/ET 边界触发模式组合
- **长连接支持**: 完整实现 HTTP 1.1 长连接、Keep-Alive 机制
- **超时管理**: 基于小根堆实现的定时器，自动关闭超时非活动连接

### HTTP 协议
- **完整 HTTP 支持**: 实现 GET 和 POST 请求方法
- **请求解析**: 利用有限状态机高效解析 HTTP 请求报文
- **请求解析**: 利用有限状态机高效解析 HTTP 请求报文
- **静态资源服务**: 支持多种文件类型（HTML、CSS、JS、图片、视频等）
- **动态路由**: 灵活的短路径映射系统（如 `/login` → `/html/login.html`）
- **错误处理**: 完善的 HTTP 状态码处理和错误页面

### 数据库
- **连接池**: 基于 RAII 机制实现的数据库连接池，默认 10 连接
- **用户管理**: 支持用户注册、登录、数据持久化
- **表单处理**: 完整的 POST 表单数据解析和 SQL 注入防护

### 日志系统
- **异步日志**: 基于单例模式和阻塞队列的高效异步日志系统
- **日志级别**: 支持 DEBUG、INFO、WARN、ERROR 四个级别
- **自动刷盘**: 可配置的日志刷盘间隔，确保日志安全

### 内存管理
- **智能指针**: 大量使用 std::unique_ptr、std::shared_ptr 自动管理内存
- **缓冲区**: 利用标准库容器封装 char，实现自动增长的循环缓冲区
- **RAII 原则**: 全面遵循资源获取即初始化原则

## 📊 性能指标

使用 webbench 压力测试：
- **QPS**: 10000+ 并发请求/秒
- **并发连接**: 支持数千并发连接
- **内存占用**: 优化的内存使用，支持长时间稳定运行

## 🏗️ 系统架构

```
┌─────────────────────────────────────────┐
│          HTTP Client Requests           │
└────────────────┬────────────────────────┘
                 │
        ┌────────▼────────┐
        │  Main Reactor   │  (Epoll 监听)
        │  (主线程)        │
        └────────┬────────┘
                 │ (Accept)
        ┌────────▼────────────────────┐
        │  Sub Reactor Thread Pool    │  
        │  (处理已连接客户端)            │
        └────────┬────────────────────┘
                 │
        ┌────────▼────────────────────┐
        │  HttpConnect (per client)   │
        │  ├─ HttpRequest Parser      │
        │  ├─ HttpResponse Builder    │
        │  └─ File Mmap Handler       │
        └────────┬────────────────────┘
                 │
    ┌────────────┼────────────────┐
    │            │                │
┌───▼───┐   ┌────▼────┐   ┌──────▼──────┐
│ Timer │   │ Logger  │   │ SQL ConnPool│
│ (堆)  │   │(异步队列)│    │             │
└───────┘   └─────────┘   └─────────────┘
                                 │
                           ┌─────▼─────┐
                           │  MySQL DB │
                           └───────────┘
```

## 📦 项目结构

```
.
├── code/                    # 源代码目录
│   ├── main.cpp            # 主入口
│   ├── buffer/             # 自动增长缓冲区
│   ├── config/             # 配置管理
│   ├── http/               # HTTP 协议处理
│   ├── log/                # 异步日志系统
│   ├── pool/               # 对象池 (线程池、数据库连接池)
│   ├── server/             # 服务器核心
│   └── timer/              # 定时器
├── resources/              # 静态资源
│   ├── html/               # HTML 页面
│   ├── css/                # 样式文件 (现代黑红主题)
│   ├── js/                 # JavaScript 脚本
│   ├── images/             # 图片资源
│   └── video/              # 视频资源
├── test/                   # 单元测试脚本
├── build/                  # 编译产物目录
├── bin/                    # 可执行文件目录
│   └── server              # HTTP 服务器可执行文件
├── log/                    # 日志文件目录
├── config.conf             # 服务器配置文件
├── init.sql                # 数据库初始化脚本
├── CMakeLists.txt          # 编译配置文件
└── README.md               # 项目文档
```

## 🔧 环境要求

- **OS**: Linux (Ubuntu 20.04+ 推荐)
- **编译器**: GCC 10+ 或 Clang 12+ (C++20 支持)
- **数据库**: MySQL 8.0+
- **构建工具**: CMake 3.10+
- **依赖库**: libmysqlclient-dev、pthread

## 📥 快速开始

### 方式一：本地编译运行

#### 1. 克隆项目
```bash
git clone https://github.com/RayTheCrack/Oxy-WebServer.git
cd Oxy-WebServer    
```

#### 2. 安装依赖
```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ make libmysqlclient-dev mysql-server
sudo systemctl start mysql
```

#### 3. 创建数据库
```bash
mysql -u root -p < init.sql
```

#### 4. 编译和运行
```bash
cmake -B build && cmake --build build
./bin/server
```

访问: `http://localhost:9999`

#### 5. 测试
```bash
# 查看日志
tail -f log/webserver.log

# 性能测试 使用 webbench 1000 个并发连接 30 秒
./webbench-1.5/webbench -c 1000 -t 30 http://localhost:9999/
```


## 🎨 前端特性

- **现代化设计**: 黑红主题，响应式布局
- **动态加载**: 图片库和视频库自动检测并展示
- **完整页面**: 首页、登录、注册、欢迎、错误等多个页面
- **自定义错误页**: 400、403、404、500 错误页面

## ⚙️ 配置说明

编辑 `config.conf`：

```properties


# 资源根目录
resource_root = resources/  
resource_root = resources/  
# 日志配置


log_file = log/webserver.log
# 0 : DEBUG 1 : INFO 2 : WARN 3 : ERROR
log_level = 3
# 日志队列最大容量
# 0 : DEBUG 1 : INFO 2 : WARN 3 : ERROR
log_level = 3
# 日志队列最大容量
log_queue_size = 1024
# 强制刷盘间隔（秒）
# 强制刷盘间隔（秒）
log_flush_interval = 3
# 是否启用日志
open_log = true
# 是否启用日志
open_log = true

# 网络配置

# 端口
# Server listening port

# 端口
# Server listening port
port = 9999
# 是否优雅关闭服务器
opt_linger = false       
# Epoll触发模式 1 : LT LT 2 : LT ET 3 : ET LT 4 : ET ET
trigger_mode = 4
# 线程池线程数量
thread_num = 32
# 最大请求体大小（字节）1MB
max_body_size = 1048576  
# 连接超时时间（秒）
# 是否优雅关闭服务器
opt_linger = false       
# Epoll触发模式 1 : LT LT 2 : LT ET 3 : ET LT 4 : ET ET
trigger_mode = 4
# 线程池线程数量
thread_num = 32
# 最大请求体大小（字节）1MB
max_body_size = 1048576  
# 连接超时时间（秒）
connection_timeout = 60

# 数据库配置

# 连接池大小
connection_pool_size = 16
db_host = 127.0.0.1
db_port = 3306
db_user = root
db_password = password
db_name = webserver

# 数据库配置

# 连接池大小
connection_pool_size = 16
db_host = 127.0.0.1
db_port = 3306
db_user = root
db_password = password
db_name = webserver
```

## 🔗 API 端点

| 路径 | 方法 | 说明 |
|------|------|------|
| `/` | GET | 首页 |
| `/login` | GET/POST | 登录 |
| `/register` | GET/POST | 注册 |
| `/picture` | GET | 图片库 |
| `/video` | GET | 视频库 |
| `/html/login.html` | POST | 登录表单提交 |
| `/html/register.html` | POST | 注册表单提交 |

## 🧪 测试

```bash
# 单元测试
./test/heaptimer_test.sh
./test/httpconnect_test.sh
./test/httprequest_test.sh
./test/httpresponse_test.sh
./test/sqlpool_test.sh

# 性能测试
./webbench-1.5/webbench -c 1000 -t 10 http://localhost:9999/
```

## 📄 许可证

MIT License

## 👨‍💻 作者

**RayTheCrack** (OXYTHECRACK)

---

**版本**: 1.0.2  
**最后更新**: 2026 年 3 月 8 日