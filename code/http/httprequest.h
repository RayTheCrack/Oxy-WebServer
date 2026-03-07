#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <errno.h>

#include "../pool/sqlconnRAII.h"
#include "../log/log.h"
#include "../buffer/buffer.h"

class HttpRequest {

public:
    // 解析Http过程的各状态，用于状态机
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest() { init(); }
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& buff); // 解析http请求

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;
    bool IsKeepAlive() const; // 是否长连接

private:
    // 解析HTTP请求行 
    bool ParseRequestLine_(const std::string line);
    // 解析HTTP请求头
    void ParseHeader_(const std::string& line);
    // 解析HTTP请求体
    void ParseBody_(const std::string& line);
    // 解析HTTP请求路径
    void ParsePath_();
    // 解析HTTP请求方法
    void ParsePost_();
    // 解析url编码
    void ParseFromUrlencoded_();
    // 解析JSON格式的请求体
    void ParseFromJson_(); 
    // 解析HTTP请求参数
    static int ConverHex(char ch);

    // 验证用户名和密码
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);
    
    PARSE_STATE state_;
    std::string method_, path_, version_, body_; // 请求行 
    std::unordered_map<std::string,std::string> header_, post_;
    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
};
#endif /* HTTPREQUEST_H */
