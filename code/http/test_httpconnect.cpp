
#include "httpconnect.h"
#include "../buffer/buffer.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 辅助函数：创建测试文件
void createTestFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}
// 辅助函数：创建测试目录和文件
void setupTestResources(const std::string& testDir) {
    // 创建测试目录
    std::filesystem::create_directories(testDir);

    // 创建测试HTML文件
    createTestFile(testDir + "/index.html", "<html><body>Index Page</body></html>");
    createTestFile(testDir + "/test.html", "<html><body>Test Page</body></html>");
    createTestFile(testDir + "/login.html", "<html><body>Login Page</body></html>");
    createTestFile(testDir + "/welcome.html", "<html><body>Welcome Page</body></html>");

    // 创建错误页面
    createTestFile(testDir + "/400.html", "<html><body>400 Bad Request</body></html>");
    createTestFile(testDir + "/403.html", "<html><body>403 Forbidden</body></html>");
    createTestFile(testDir + "/404.html", "<html><body>404 Not Found</body></html>");
    createTestFile(testDir + "/500.html", "<html><body>500 Internal Server Error</body></html>");
}

// 辅助函数：清理测试资源
void cleanupTestResources(const std::string& testDir) {
    std::filesystem::remove_all(testDir);
}

// 辅助函数：创建测试socket对
std::pair<int, int> createSocketPair() {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        LOG_ERROR("socketpair failed: {}", strerror(errno));
        return {-1, -1};
    }
    // 设置socket为非阻塞模式
    for(int i = 0; i < 2; i++) {
        int flags = fcntl(sv[i], F_GETFL, 0);
        if(flags == -1) {
            LOG_ERROR("fcntl F_GETFL failed: {}", strerror(errno));
            close(sv[0]);
            close(sv[1]);
            return {-1, -1};
        }
        if(fcntl(sv[i], F_SETFL, flags | O_NONBLOCK) == -1) {
            LOG_ERROR("fcntl F_SETFL failed: {}", strerror(errno));
            close(sv[0]);
            close(sv[1]);
            return {-1, -1};
        }
    }
    return {sv[0], sv[1]};
}

// 测试1: 基本连接初始化和关闭
void testBasicConnection() {
    LOG_INFO("=== Test 1: Basic Connection Init and Close ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 验证初始化
    assert(conn.get_fd() == server_fd);
    assert(HttpConnect::user_count.load() == 1);

    // 关闭连接
    conn.close();
    assert(HttpConnect::user_count.load() == 0);

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 1 passed!");
    cleanupTestResources(testDir);
}

// 测试2: 读取HTTP请求
void testReadRequest() {
    LOG_INFO("=== Test 2: Read HTTP Request ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送HTTP请求
    std::string request = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);

    // 读取请求
    // 使用临时缓冲区
    int tmp_err = 0;
    ssize_t read_bytes = conn.read(server_fd, conn.getReadBuffer(), tmp_err);

    // 验证读取结果
    assert(read_bytes > 0);
    assert(conn.getReadBuffer().readable_size() == request.size());

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 2 passed!");
    cleanupTestResources(testDir);
}

// 测试3: 处理HTTP请求
void testProcessRequest() {
    LOG_INFO("=== Test 3: Process HTTP Request ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送HTTP请求
    std::string request = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);

    // 读取请求
    // 读取请求到HttpConnect的内部缓冲区
    int tmp_err = 0;
    ssize_t read_bytes = conn.read(server_fd, conn.getReadBuffer(), tmp_err);
    assert(read_bytes > 0);

    // 处理请求
    bool process_result = conn.process();
    assert(process_result == true);

    // 验证响应已生成
    assert(conn.to_write_bytes() > 0);

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 3 passed!");
    cleanupTestResources(testDir);
}

// 测试4: 健康检查路径
void testHealthEndpoint() {
    LOG_INFO("=== Test 4: Health Endpoint ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);
    HttpConnect::srcDir = testDir.c_str();

    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);
    sockaddr_in client_addr{};
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    HttpConnect conn;
    conn.init(server_fd, client_addr);

    std::string request = "GET /health HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);
    int tmp_err = 0;
    ssize_t read_bytes = conn.read(server_fd, conn.getReadBuffer(), tmp_err);
    assert(read_bytes > 0);

    bool process_result = conn.process();
    assert(process_result == true);
    assert(conn.to_write_bytes() > 0);

    // 检查返回内容是否包含 'healthy'
    std::string response(conn.getWriteBuffer().peek(), conn.to_write_bytes());
    assert(response.find("healthy") != std::string::npos);

    conn.close();
    close(client_fd);
    LOG_INFO("✓ Test 4 passed!");
    cleanupTestResources(testDir);
}

// 测试4: 写入HTTP响应
void testWriteResponse() {
    LOG_INFO("=== Test 4: Write HTTP Response ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送HTTP请求
    std::string request = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);

    // 读取请求
    // 使用临时缓冲区
    ssize_t read_bytes = conn.read(server_fd, conn.getReadBuffer());
    assert(read_bytes > 0);

    // 处理请求
    bool process_result = conn.process();
    assert(process_result == true);

    // 写入响应（使用writeResponse方法）
    ssize_t write_bytes = conn.writeResponse(server_fd);
    assert(write_bytes >= 0);

    // 从客户端读取响应
    char response_buff[4096] = {0};
    ssize_t recv_bytes = recv(client_fd, response_buff, sizeof(response_buff), 0);
    assert(recv_bytes > 0);

    // 验证响应内容
    std::string response(response_buff, recv_bytes);
    std::cout << "Response: " << response << std::endl;
    assert(response.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(response.find("Index Page") != std::string::npos);

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 4 passed!");
    cleanupTestResources(testDir);
}

// 测试5: Keep-Alive连接
void testKeepAliveConnection() {
    LOG_INFO("=== Test 5: Keep-Alive Connection ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送第一个请求（Keep-Alive）
    std::string request1 = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
    send(client_fd, request1.c_str(), request1.size(), 0);

    // 读取并处理第一个请求
    // 使用临时缓冲区
    int tmp_err = 0;
    conn.read(server_fd, conn.getReadBuffer(), tmp_err);
    conn.process();

    // 验证Keep-Alive
    assert(conn.IsKeepAlive() == true);

    // 发送第二个请求
    conn.getReadBuffer().clear();
    std::string request2 = "GET /test.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
    send(client_fd, request2.c_str(), request2.size(), 0);

    // 读取并处理第二个请求
    {
        int tmp_err2 = 0;
        conn.read(server_fd, conn.getReadBuffer(), tmp_err2);
    }
    conn.process();

    // 验证第二个请求也成功处理
    assert(conn.to_write_bytes() > 0);

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 5 passed!");
    cleanupTestResources(testDir);
}

// 测试6: 404错误处理
void testNotFoundError() {
    LOG_INFO("=== Test 6: 404 Not Found Error ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送请求不存在的文件
    std::string request = "GET /nonexistent.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);

    // 读取并处理请求
    // 使用临时缓冲区
    conn.read(server_fd, conn.getReadBuffer());
    bool process_result = conn.process();
    assert(process_result == true);

    // 写入响应（使用writeResponse方法）
    conn.writeResponse(server_fd);

    // 从客户端读取响应
    char response_buff[4096] = {0};
    ssize_t recv_bytes = recv(client_fd, response_buff, sizeof(response_buff), 0);
    assert(recv_bytes > 0);

    // 验证404错误响应
    std::string response(response_buff, recv_bytes);
    assert(response.find("HTTP/1.1 404 Not Found") != std::string::npos);

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 6 passed!");
    cleanupTestResources(testDir);
}

// 测试7: 边缘触发模式
void testEdgeTriggerMode() {
    LOG_INFO("=== Test 7: Edge Trigger Mode ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 设置为边缘触发模式
    HttpConnect::is_edge_trigger = true;

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送HTTP请求
    std::string request = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);

    // 读取请求
    // 使用临时缓冲区
    ssize_t read_bytes = conn.read(server_fd, conn.getReadBuffer());
    assert(read_bytes > 0);

    // 处理请求
    bool process_result = conn.process();
    assert(process_result == true);

    // 写入响应（使用writeResponse方法）
    ssize_t write_bytes = conn.writeResponse(server_fd);
    assert(write_bytes >= 0);

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    // 恢复水平触发模式
    HttpConnect::is_edge_trigger = false;

    LOG_INFO("✓ Test 7 passed!");
    cleanupTestResources(testDir);
}

// 测试8: 获取连接信息
void testGetConnectionInfo() {
    LOG_INFO("=== Test 8: Get Connection Info ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 验证连接信息
    assert(conn.get_fd() == server_fd);
    assert(std::string(conn.get_ip()) == "127.0.0.1");
    assert(conn.get_port() == 8080);

    // 验证地址结构
    sockaddr_in addr = conn.get_addr();
    assert(addr.sin_family == AF_INET);
    assert(addr.sin_port == htons(8080));

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 8 passed!");
    cleanupTestResources(testDir);
}

// 测试9: 多次请求处理
void testMultipleRequests() {
    LOG_INFO("=== Test 9: Multiple Requests ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送多个请求
    for (int i = 0; i < 3; i++) {
        // 发送请求
        std::string request = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
        send(client_fd, request.c_str(), request.size(), 0);

        // 读取并处理请求
        // 使用临时缓冲区
        conn.read(server_fd, conn.getReadBuffer());
        bool process_result = conn.process();
        assert(process_result == true);

        // 写入响应（使用writeResponse方法）
        conn.writeResponse(server_fd);

        // 从客户端读取响应
        char response_buff[4096] = {0};
        recv(client_fd, response_buff, sizeof(response_buff), 0);
    }

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 9 passed!");
    cleanupTestResources(testDir);
}

// 测试10: 无效请求处理
void testInvalidRequest() {
    LOG_INFO("=== Test 10: Invalid Request ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 初始化静态资源目录
    HttpConnect::srcDir = testDir.c_str();

    // 创建测试socket
    auto [client_fd, server_fd] = createSocketPair();
    assert(client_fd >= 0 && server_fd >= 0);

    // 创建客户端地址
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &client_addr.sin_addr);

    // 初始化HttpConnect
    HttpConnect conn;
    conn.init(server_fd, client_addr);

    // 发送无效请求
    std::string request = "INVALID REQUEST\r\nHost: localhost:8080\r\n\r\n";
    send(client_fd, request.c_str(), request.size(), 0);

    // 读取并处理请求
    // 使用临时缓冲区
    conn.read(server_fd, conn.getReadBuffer());
    bool process_result = conn.process();
    assert(process_result == false);

    // 写入响应（使用conn内部的writeBuff_）
    conn.write(server_fd, conn.getWriteBuffer());

    // 从客户端读取响应
    char response_buff[4096] = {0};
    ssize_t recv_bytes = recv(client_fd, response_buff, sizeof(response_buff), 0);
    assert(recv_bytes > 0);

    // 验证400错误响应
    std::string response(response_buff, recv_bytes);
    assert(response.find("HTTP/1.1 400 Bad Request") != std::string::npos);

    // 关闭连接
    conn.close();

    // 清理socket
    close(client_fd);

    LOG_INFO("✓ Test 10 passed!");
    cleanupTestResources(testDir);
}

int main() {
    // 初始化日志系统
    Logger::getInstance().initLogger("log/httpconnect.log", LogLevel::DEBUG, 1024);

    LOG_INFO("Starting HTTP Connect Tests...");
    LOG_INFO("===============================");

    try {
        testBasicConnection();
        testReadRequest();
        testProcessRequest();
        testWriteResponse();
        testKeepAliveConnection();
        testNotFoundError();
        testEdgeTriggerMode();
        testGetConnectionInfo();
        testMultipleRequests();
        testInvalidRequest();

        LOG_INFO("===============================");
        LOG_INFO("All tests passed successfully! ✓");
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        Logger::getInstance().shutdown();
        return 1;
    }

    Logger::getInstance().shutdown();
    return 0;
}
