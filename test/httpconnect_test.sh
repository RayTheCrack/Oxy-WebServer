
#!/bin/bash

# HTTP连接测试程序

g++ -std=c++23 -Wall -Wextra -O2 -pthread \
    -I./code \
    -o bin/test_httpconnect \
    code/http/test_httpconnect.cpp \
    code/http/httpconnect.cpp \
    code/http/httprequest.cpp \
    code/http/httpresponse.cpp \
    code/pool/sqlconnpool.cpp \
    code/config/config.cpp \
    code/log/log.cpp \
    code/buffer/buffer.cpp \
    -lmysqlclient

echo "编译完成！运行测试程序："
echo "./bin/test_httpconnect"
