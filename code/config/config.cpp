#include "config.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}
/*
    getopt 工作原理（执行流程）
    以你的代码 while ((opt = getopt(argc, argv, "p:t:r:")) != -1) 为例，执行流程是：
    第一次循环：getopt 从 argv[1] 开始扫描，找到第一个参数（如 -p），检查是否在 optstring 中；
    验证规则：如果是 -p（对应 p:），则读取后续的 8080 作为值，存入 optarg，返回 'p'；
    自动移动索引：optind 自动跳到下一个未解析的参数（比如解析完 -p 8080 后，optind=3）；
    循环解析：直到扫描完所有参数，getopt 返回 -1，循环结束；
    非法处理：如果遇到 -h（不在 optstring 中），返回 ?，触发 default 分支，输出用法并退出。
*/

// 注意：命令台参数只支持修改端口和线程数，其他配置项需要通过配置文件修改。
void Config::parse_args(int argc, char* argv[]) {
    int opt;
    while((opt = getopt(argc, argv, "p:t:")) != -1) {
        switch(opt) {
            case 'p': {
                uint16_t _port = static_cast<uint16_t>(std::stoi(optarg));
                if(_port == 0) {
                    std::cerr << "[ERROR] Invalid port number: " << optarg << std::endl;
                    exit(EXIT_FAILURE);
                }
                c_port = _port;
                break;
            }
            case 't': {
                int _thread_cnt = std::stoi(optarg);
                if(_thread_cnt <= 0) {
                    std::cerr << "[ERROR] Invalid thread count: " << optarg << std::endl;
                    exit(EXIT_FAILURE);
                }
                c_thread_cnt = _thread_cnt;
                break;
            
            }
            default: {
                std::cerr << "Usage: " << argv[0] << " [-p port] [-t thread_count]" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }
}

void Config::parse_config_file(const std::string& filePath) {
    std::ifstream file(filePath);
    if(!file.is_open()) {
        std::cerr << "[WARN] Config file not found: " << filePath << ", using defaults" << std::endl;
        return;  // return 允许使用默认配置
    }
    std::string line;
    while(std::getline(file, line)) {
        if(line.empty() or line[0] == '#') continue; // 跳过空行和注释
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // 移除键值两端的空格/制表符（格式化）
            // 移除开头的空格/制表符
            key.erase(0, key.find_first_not_of(" \t"));
            // 移除结尾的空格/制表符（find_last_not_of返回最后一个非空白符位置，+1后erase到末尾）
            key.erase(key.find_last_not_of(" \t") + 1);
            // 对value做同样的格式化
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);


            if (key == "port") c_port = std::stoi(value);
            else if (key == "thread_num") c_thread_cnt = std::stoi(value);
            else if (key == "resource_root") c_resource_root = value;
            else if (key == "log_file") c_log_file = value;
            else if (key == "log_flush_interval") c_log_flush_interval = std::stoi(value);
            else if (key == "open_log") c_open_log = (value == "true" or value == "1");
            else if (key == "log_queue_size") c_log_queue_size = std::stoi(value);
            else if (key == "opt_linger") c_isOptLinger = (value == "true" or value == "1");
            else if (key == "trigger_mode") c_trigMode = std::stoi(value);
            else if (key == "max_connections") c_maxConnection = std::stoi(value);
            else if (key == "log_level") c_log_level = std::stoi(value);
            else if (key == "max_body_size") c_max_body_size = std::stoi(value);
            else if (key == "connection_timeout") c_timeout = std::stoi(value) * 1000; // 转换为毫秒
            else if (key == "db_host") c_db_host = value;
            else if (key == "db_port") c_db_port = std::stoi(value);
            else if (key == "db_user") c_db_user = value;
            else if (key == "db_password") c_db_password = value;
            else if (key == "db_name") c_db_name = value;
            else if (key == "connection_pool_size") c_conn_pool_num = std::stoi(value);

        }
    }
    file.close();
}

void Config::print_config() const {
    std::cout << "=== Current Configuration ===" << std::endl;
    std::cout << "Port: " << c_port << std::endl;
    std::cout << "Trigger Mode: " << (c_trigMode == 1 ? "LT" : "ET") << std::endl;
    std::cout << "Max Connections: " << c_maxConnection << std::endl;
    std::cout << "Opt Linger: " << (c_isOptLinger ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Thread Count: " << c_thread_cnt << std::endl;
    std::cout << "Resource Root: " << c_resource_root << std::endl;
    std::cout << "Open Log: " << (c_open_log ? "Yes" : "No") << std::endl;    
    std::cout << "Log Queue Size: " << c_log_queue_size << std::endl; 
    std::cout << "Log File: " << c_log_file << std::endl;
    std::cout << "Log Level: " << c_log_level << std::endl;
    std::cout << "Log Flush Interval: " << c_log_flush_interval << " seconds" << std::endl;
    std::cout << "Max Body Size: " << c_max_body_size / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Connection Timeout: " << c_timeout << " seconds" << std::endl;
    std::cout << "Connection Pool Num: " << c_conn_pool_num << std::endl;
    std::cout << "Database Host: " << c_db_host << std::endl;
    std::cout << "Database Port: " << c_db_port << std::endl;
    std::cout << "Database User: " << c_db_user << std::endl;
    // 不建议打印数据库密码哈 =.=
    // std::cout << "Database Password: " << db_password << std::endl; 
    std::cout << "Database Name: " << c_db_name << std::endl;
}