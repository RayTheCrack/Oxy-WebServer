
cmake_minimum_required(VERSION 3.10)

# 项目名称 
project(OxyWebServer)

# C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    code/main.cpp
    code/config/config.cpp
    code/log/log.cpp
    code/buffer/buffer.cpp
    code/http/httprequest.cpp
    code/pool/sqlconnpool.cpp
    code/http/httpresponse.cpp
    code/http/httpconnect.cpp
    code/server/epoller.cpp
    code/timer/heaptimer.cpp
    code/pool/threadpool.cpp
    code/server/webserver.cpp
)

# 创建可执行文件, 输出到bin目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
add_executable(server ${SOURCES})

# 添加编译选项（线程库+MySQL库）
target_compile_options(server PRIVATE -Wall -Wextra -O2 -pthread)
target_link_libraries(server PRIVATE pthread -lmysqlclient)
