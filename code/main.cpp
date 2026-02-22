#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>

#include "config/config.h"
#include "log/log.h"

int main(int argc, char* argv[]) {

    // 获取配置单例
    Config& config = Config::getInstance();

    // 1. 首先解析配置文件（基础配置）
    config.parse_config_file("config.conf");

    // 2. 再解析命令行参数（高优先级，可覆盖文件配置）
    config.parse_args(argc, argv);

    // 初始化日志系统（使用 config 中的配置）
    Logger::getInstance().initLogger(config.c_log_file, static_cast<LogLevel>(config.c_log_level),
    config.c_log_queue_size, config.c_log_flush_interval);

    Logger::getInstance().shutdown();
    return 0;
}
