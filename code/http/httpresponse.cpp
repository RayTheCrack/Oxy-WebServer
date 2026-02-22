#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/msword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css"},
    { ".js",    "text/javascript"},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS {
    { 200, "OK"},
    { 400, "Bad Request"},
    { 403, "Forbidden"},
    { 404, "Not Found"},
    { 500, "Internal Server Error"},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH {
    { 400, "/400.html"},
    { 403, "/403.html"},
    { 404, "/404.html"},
    { 500, "/500.html"},
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code) {
    assert(srcDir != ""); // 断言srcDir不为空
    if(mmFile_) { UnmapFile();} // 如果mmFile_不为空，则解除映射
    srcDir_ = srcDir;
    path_ = path;
    isKeepAlive_ = isKeepAlive;
    code_ = code;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
};

void HttpResponse::MakeResponse(Buffer& buff) {
    // 只有在code_为-1时才调用stat获取文件信息
    if(code_ == -1) {
        // stat()获取文件的元信息（大小、权限、类型等）并写入 mmFileStat_
        if(stat((srcDir_ + path_).c_str(), &mmFileStat_) < 0 or S_ISDIR(mmFileStat_.st_mode))
    /*
        S_ISDIR() 是宏定义，用于判断 mmFileStat_.st_mode（文件模式）是否表示 “目录”；
        这段代码的逻辑是：不允许直接访问目录，如果请求的路径是目录（比如 /static/），则返回 404；
    */
            code_ = 404; // stat返回-1，说明文件不存在
        else if(!(mmFileStat_.st_mode & S_IROTH)) // 如果文件不可读
            code_ = 403; // 如果文件不可读，则为403 Forbidden
        else // 如果code_为-1
                code_ = 200;
    }
    ErrorHtml_(); // 状态码为错误码时，将请求路径替换为预设的错误页面路径，并重新获取错误页面的文件信息。
    AddStateLine_(buff);// 添加状态行
    AddHeaders_(buff); // 添加响应头
    AddBody_(buff); // 添加响应体
}

char* HttpResponse::GetFile() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size; // 返回文件大小
}

// 当 HTTP 响应状态码为错误码（如 404/403/500）时，
// 将请求路径替换为预设的错误页面路径，并重新获取错误页面的文件信息。
void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_)) {
        path_ = CODE_PATH.find(code_)->second;
        // 重新获取错误页面的文件信息，写入mmFileStat_
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer& buff) {
    std::string status;
    if(CODE_STATUS.count(code_)) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(code_)->second;
    }
    buff.append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeaders_(Buffer& buff) {
    buff.append("Connection: ");
    if(isKeepAlive_) {
       buff.append("keep-alive\r\n");
       buff.append("Keep-Alive: max=6, timeout=120\r\n"); 
    }
    else {
        buff.append("close\r\n");
    }
    buff.append("Content-Type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddBody_(Buffer& buff) {
    int srcFD = open((srcDir_ + path_).data(), O_RDONLY | O_CLOEXEC);
    if(srcFD == -1) {
        ErrorContent(buff, "File Not Found!");
        return;
    }
    LOG_DEBUG("File path: {}",std::string(srcDir_ + path_));
    void* mmRet =  mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFD, 0);
    if(mmRet == MAP_FAILED) {
        ErrorContent(buff, "File Mmap Failed!");
        return;
    }   
    mmFile_ = static_cast<char*>(mmRet);
    close(srcFD); // 关闭原文件不影响已存在的内存映射
    buff.append("Content-Length: " + std::to_string(mmFileStat_.st_size) + "\r\n"); 
    buff.append("\r\n");
    // 文件内容不添加到缓冲区，而是通过iov[1]直接发送
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

std::string HttpResponse::GetFileType_() {
    std::string::size_type idx = path_.find_last_of('.');
    // 无后缀，默认返回纯文本类型
    if(idx == std::string::npos) return "text/plain";
    std::string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix)) {
        // 找到则返回对应的 MIME 类型（比如 .html → "text/html"）
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message) {
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_)) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status + "<br/><br/>";
    body += "<p>" + message + "</p>";
    body += "<hr><em>WebServer</em></body></html>";
    buff.append("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.append(body); 
}