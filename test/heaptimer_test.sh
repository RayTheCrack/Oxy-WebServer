#!/bin/bash

# HeapTimer 定时器测试程序

g++ -std=c++20 -Wall -Wextra -O2 -pthread \
    -I./code \
    -o bin/test_heaptimer \
    code/timer/test_heaptimer.cpp \
    code/timer/heaptimer.cpp \
    code/log/log.cpp \
    code/buffer/buffer.cpp \
    -lmysqlclient

echo "编译完成！运行测试程序："
echo "./bin/test_heaptimer"
