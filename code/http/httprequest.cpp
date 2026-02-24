#include "httprequest.h"
#include <algorithm>

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/login", "/register", "/index",
    "/welcome", "/video", "/picture" 
};


const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1},  
};

void HttpRequest::init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        // header_用const修饰，不支持使用[]运算符，只能使用find函数
        return header_.find("Connection")->second == "keep-alive" and version_ == "1.1";
    }
    return false;
}
/**
 * @brief 解析HTTP请求的函数，基于状态机逐行解析请求行、请求头和请求体
 * @details HTTP请求的标准格式如下：
 *          请求行（GET / HTTP/1.1\r\n）
 *          请求头1（Host: localhost\r\n）
 *          请求头2（Content-Length: 0\r\n）
 *          空行（\r\n）  → 请求头结束标志
 *          请求体（可选，POST 请求才有）
 * @param buff 输入的缓冲区对象，包含待解析的HTTP原始字节数据；要求缓冲区已初始化且有可读数据
 * @return true 表示解析流程正常执行（包括“数据不足需等待”“解析完成”等场景）；false 表示解析出错（如请求行格式非法、缓冲区为空）
 */
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n"; // Http请求行的结束标志
    if(buff.readable_size() <= 0) return false;
    while(buff.readable_size() and state_ != FINISH) {
        const char* line_end = std::search(buff.peek(), buff.begin_write_const( ), CRLF, CRLF + 2);
        std::string line(buff.peek(), line_end);
        switch(state_) {
            case REQUEST_LINE:
                if(!ParseRequestLine_(line))  return false;
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if(buff.readable_size() <= 2) state_ = FINISH;
                break;
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if(line_end == buff.begin_write_const()) break;
        buff.retrieve_until(line_end + 2);
    }
    LOG_DEBUG("[{}], [{}], [{}]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

// 请求行形式 ： GET / HTTP/1.1
bool HttpRequest::ParseRequestLine_(const std::string line) {
    // 使用字符串查找代替正则表达式，提高效率
    size_t method_end = line.find(' ');
    if(method_end == std::string::npos) {
        LOG_ERROR("RequestLine Error : {}", line.c_str());
        return false;
    }
    size_t path_end = line.find(' ', method_end + 1);
    if(path_end == std::string::npos) {
        LOG_ERROR("RequestLine Error : {}", line.c_str());
        return false;
    }
    // 查找 HTTP/ 前缀
    size_t http_pos = line.find("HTTP/", path_end + 1);
    if(http_pos == std::string::npos) {
        LOG_ERROR("RequestLine Error : {}", line.c_str());
        return false;
    }
    method_ = line.substr(0, method_end);
    path_ = line.substr(method_end + 1, path_end - method_end - 1);
    version_ = line.substr(http_pos + 5);  // 跳过 "HTTP/"
    state_ = HEADERS;
    return true;
}

// Header 形式 : key: value
void HttpRequest::ParseHeader_(const std::string& line) {
    // 使用字符串查找代替正则表达式，提高效率
    size_t colon_pos = line.find(':');
    if(colon_pos != std::string::npos) {
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        // 去除value开头的空格
        size_t value_start = value.find_first_not_of(" \t");
        if(value_start != std::string::npos) {
            value = value.substr(value_start);
        } else {
            value.clear();  // 如果全是空格，则清空value
        }
        
        header_[key] = value;
    }
    else state_ = BODY;
}      

void HttpRequest::ParsePath_() {
    // Default to the html folder for the UI pages
    if(path_ == "/") path_ = "/index.html";
    else {
        // 检查是否是静态资源请求（CSS、JS、图片等）
        if(path_.find("/css/") == 0 || path_.find("/js/") == 0 || 
           path_.find("/images/") == 0 || path_.find("/fonts/") == 0) {
            // 静态资源保持原路径
            return;
        }
        
        // 检查是否是预定义的HTML页面
        for(auto &item : DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body : {}, len : {}", line.c_str(), line.size());
}
/**
 * @brief 解析POST请求的函数
 * 该函数处理POST请求，特别是Content-Type为application/x-www-form-urlencoded的请求
 * 它会解析表单数据，并根据请求路径进行相应的用户验证
 */
void HttpRequest::ParsePost_() {
    LOG_DEBUG("ParsePost_ called, method_: {}, content-type: {}", 
              method_.c_str(), header_["Content-Type"].c_str());
    // 检查请求方法是否为POST且Content-Type是否为表单编码类型
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        LOG_DEBUG("Valid POST request detected, parsing form data...");
        // 解码URL编码的POST数据
        ParseFromUrlencoded_();
        LOG_DEBUG("Form data parsed, path_: {}", path_.c_str());
        // 检查当前路径是否在预定义的HTML标签映射中
        if(DEFAULT_HTML_TAG.count(path_)) {
            // 获取对应的标签值
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag: {}", tag);
            // 根据标签值判断是登录还是注册操作
            if(tag == 0 or tag == 1) {
                // 设置是否为登录操作的标志
                bool isLogin = (tag == 1);
                LOG_DEBUG("Calling UserVerify for user: {}, login: {}", 
                         post_["username"].c_str(), isLogin);
                // 验证用户名和密码
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    // 验证成功，重定向到欢迎页面
                    path_ = "/welcome.html";
                    LOG_DEBUG("User verification successful, redirecting to welcome.html");
                } 
                else {
                    // 验证失败，重定向到错误页面
                    path_ = "/error.html";
                    LOG_DEBUG("User verification failed, redirecting to error.html");
                }
            }
        } else {
            LOG_DEBUG("Path {} not found in DEFAULT_HTML_TAG", path_.c_str());
        }
    } else {
        LOG_DEBUG("Not a valid POST form request - method: {}, content-type: {}", 
                  method_.c_str(), header_["Content-Type"].c_str());
    }
}

/**
 * 将十六进制字符转换为对应的整数值
 * @param ch 需要转换的字符
 * @return 返回转换后的整数值，如果不是有效的十六进制字符则返回字符本身
 */
int HttpRequest::ConverHex(char ch) {
    // 如果字符是数字 0-9，则转换为对应的整数值 (0-9)
    if(ch >= '0' && ch <= '9') return ch - '0';
    // 如果字符是大写字母 A-F，则转换为对应的整数值 (10-15)
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    // 如果字符是小写字母 a-f，则转换为对应的整数值 (10-15)
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    // 如果字符不是有效的十六进制字符，则返回-1
    return -1;
}

/**
 * @brief 从URL编码的请求体中解析键值对
 * 该函数处理URL编码格式的请求体，将其转换为键值对并存入post_映射中
 * 支持处理普通字符、加号空格、百分号编码以及特殊字符分隔符
 */
void HttpRequest::ParseFromUrlencoded_() {
    if (body_.empty()) { return; }  // 如果请求体为空，直接返回
    std::string key, value;  // 用于存储当前解析的键和值
    int n = body_.size();    // 获取请求体长度
    int i = 0, j = 0;        // i为当前遍历位置，j为当前键值对的起始位置
    while (i < n) {
        char ch = body_[i];
        switch (ch) {
            case '=':  // 遇到等号，表示键的结束
                if(j < i) key = body_.substr(j, i - j);  // 提取键
                j = i + 1;  // 更新值的起始位置
                i++;
                break;
            
            case '+':  // 加号表示空格
                // 先添加从j到当前位置的普通字符
                if(j < i) value += body_.substr(j, i - j);
                value += ' ';  // 将加号转换为空格
                j = i + 1;
                i++;
                break;
            
            case '%': {  // 加花括号避免变量重定义
                // 边界校验：确保 % 后有至少两位字符
                if(i + 2 >= n) {
                    i++;
                    break;
                }
                // 先添加从j到当前位置的普通字符
                if(j < i) value += body_.substr(j, i - j);
                int high = ConverHex(body_[i + 1]);
                int low = ConverHex(body_[i + 2]);
                // 校验十六进制字符合法性
                if (high < 0 or low < 0) {
                    i += 3;
                    j = i;
                    break;
                }
                // 计算正确的 ASCII 字符
                char decoded = high * 16 + low;
                value += decoded;
                i += 3;  // 跳过 %xx 三位
                j = i;
                break;
            }
            case '&':
                // 截取 value，避免空 value
                if (j < i and !key.empty()) {
                    value += body_.substr(j, i - j);
                    post_[key] = value;
                    LOG_DEBUG("[key ,value] = [{} : {}]", key.c_str(), value.c_str());
                    // 重置 key/value，准备下一个键值对
                    key.clear();
                    value.clear();
                }
                j = i + 1;
                i++;
                break;
            
            default:
                // 普通字符不做处理，直接让i继续前进
                i++;
                break;
        }
    }
    // 处理最后一个键值对
    if (!key.empty() && j < n) {
        value += body_.substr(j, n - j);
        post_[key] = value;
    }
}
std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) return post_.find(key)->second;
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) return post_.find(key)->second;
    return "";
}

bool HttpRequest::UserVerify(const std::string& name, const std::string& password, bool isLogin) {
    if(name == "" or password == "") return false;
    // 验证用户名格式：只允许字母、数字和下划线
    for(char ch : name) {
        if(!((ch >= 'a' && ch <= 'z') or
           (ch >= 'A' && ch <= 'Z') or 
           (ch >= '0' && ch <= '9') or 
           ch == '_')) {
            LOG_DEBUG("Invalid username format: {}", name);
            return false;
        }
    }
    LOG_INFO("User Verifying: {}", name);
    MYSQL* sql = nullptr;
    SqlConnRAII(&sql, &SqlConnPool::getInstance()); // 从连接池中获取数据库连接
    assert(sql);
    // 验证结果标记，默认FALSE
    bool flag = false;
    char query[256] = {0}; // 查询语句缓冲区
    // MySQL字段结构指针（存储查询结果的字段信息）
    // MYSQL_FIELD* fields = nullptr;
    // MySQL结果集指针（存储查询返回的数据）
    MYSQL_RES* res = nullptr;
    // 注册场景默认标记为true（假设用户名未被占用，后续查询后修正）
    if(!isLogin) flag = true;
    // snprintf : 格式化字符串，将查询语句格式化到query中，此处有SQL注入风险，需注意
    snprintf(query, 256, "SELECT username, password FROM user WHERE username = '%s' LIMIT 1",name.c_str());
    LOG_DEBUG("{}", query); // 打印查询语句

    // 执行查询语句，如果失败则释放结果集并返回false，注意：mysql_query返回0表示成功
    if(mysql_query(sql, query)) {
        LOG_ERROR("MySQL query failed: {}", mysql_error(sql));
        if(res) mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(sql); // 执行查询并存储结果
    // cnt = mysql_num_fields(res); // 获取查询结果中的字段数量
    // fields = mysql_fetch_fields(res); // 获取字段信息

    // 遍历查询结果
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: {} {}", row[0], row[1]);
        std::string pwd(row[1]); // 获取数据库中存储的密码
        if(isLogin) {
            if(password == pwd) flag = true; // 登录时，密码匹配则标记为true
            else {
                flag = false;
                LOG_DEBUG("Password is wrong!");
            }
        } //  注册场景：查询到记录→用户名已存在，验证失败
        else {
            flag = false;
            LOG_DEBUG("Username already exists!");
        }
    }       
    mysql_free_result(res); // 释放结果集
    // 注册行为，且用户名未被占用，则将用户名和密码插入数据库
    if(!isLogin and flag) {
        LOG_DEBUG("Registering user: {}", name);
        bzero(query, 256); // 清空缓冲区
        snprintf(query, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), password.c_str());
        LOG_DEBUG("{}", query);
        if(mysql_query(sql, query)) {
            LOG_ERROR("Insert user failed: {}", mysql_error(sql));
            flag = false;
        }
        else {
            LOG_INFO("Inserted user: {}", name);
            flag = true;
        }
    }
    // 无需手动释放，RAII机制会自动释放，手动释放可能造成资源重复释放
    // SqlConnPool::getInstance().freeConnection(sql); // 释放数据库连接
    if(flag) LOG_INFO("User {} Verify Successful!!", name);
    else LOG_WARN("User {} Verify Failed.", name);
    return flag;
}





