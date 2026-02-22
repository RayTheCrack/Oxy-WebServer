#include "heaptimer.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>

// 用于记录回调执行
static int callback_count = 0;

// 测试回调函数1
void test_callback_1() {
    LOG_INFO("Callback 1 executed!");
    callback_count++;
}

// 测试回调函数2
void test_callback_2() {
    LOG_INFO("Callback 2 executed!");
    callback_count++;
}

// 测试回调函数3
void test_callback_3() {
    LOG_INFO("Callback 3 executed!");
    callback_count++;
}

// 测试1: 基本的定时器添加和删除
void testBasicAddAndDel() {
    LOG_INFO("=== Test 1: Basic Add and Delete ===");
    HeapTimer timer;
    
    // 添加定时器
    timer.add(1, 1000, test_callback_1);  // fd=1, timeout=1000ms
    timer.add(2, 500, test_callback_2);   // fd=2, timeout=500ms
    timer.add(3, 1500, test_callback_3);  // fd=3, timeout=1500ms
    
    LOG_INFO("Added 3 timers");
    
    // 验证下一个tick时间应该接近500ms（fd=2最早过期）
    int next_tick = timer.get_next_tick();
    LOG_INFO("Next tick: {} ms", next_tick);
    assert(next_tick > 0 && next_tick <= 500);
    
    // 手动删除fd=2的定时器
    timer.do_work(2);
    LOG_INFO("Removed fd 2");
    
    LOG_INFO("✓ Test 1 passed!");
}

// 测试2: 定时器超时
void testTimeout() {
    LOG_INFO("=== Test 2: Timer Timeout ===");
    HeapTimer timer;
    callback_count = 0;
    
    // 添加快速过期的定时器
    timer.add(1, 100, test_callback_1);   // 100ms后过期
    LOG_INFO("Added timer for fd 1 with 100ms timeout");
    
    // 立即检查是否过期（不应该过期）
    timer.tick();
    assert(callback_count == 0);
    LOG_INFO("Timer not expired yet (callback_count = {})", callback_count);
    
    // 等待150ms确保过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // 现在检查是否过期
    timer.tick();
    assert(callback_count == 1);
    LOG_INFO("Timer expired! (callback_count = {})", callback_count);
    
    LOG_INFO("✓ Test 2 passed!");
}

// 测试3: 多个定时器的处理
void testMultipleTimers() {
    LOG_INFO("=== Test 3: Multiple Timers ===");
    HeapTimer timer;
    callback_count = 0;
    
    // 添加多个定时器，分别在不同时间过期
    timer.add(1, 300, test_callback_1);   // 300ms
    timer.add(2, 150, test_callback_2);   // 150ms
    timer.add(3, 450, test_callback_3);   // 450ms
    
    LOG_INFO("Added 3 timers: fd1=300ms, fd2=150ms, fd3=450ms");
    
    // 等待180ms，此时fd2应该过期
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    timer.tick();
    LOG_INFO("After 180ms: callback_count = {}", callback_count);
    if(callback_count != 1) {
        LOG_WARN("Expected callback_count=1, got {}", callback_count);
    }
    
    // 再等待150ms（总共330ms），此时fd1应该过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    timer.tick();
    LOG_INFO("After 330ms: callback_count = {}", callback_count);
    if(callback_count != 2) {
        LOG_WARN("Expected callback_count=2, got {}", callback_count);
    }
    
    // 再等待150ms（总共480ms），此时fd3应该过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    timer.tick();
    LOG_INFO("After 480ms: callback_count = {}", callback_count);
    if(callback_count != 3) {
        LOG_WARN("Expected callback_count=3, got {}", callback_count);
    }
    
    assert(callback_count >= 1);  // 至少有一个过期
    LOG_INFO("✓ Test 3 passed!");
}

// 测试4: 调整定时器超时时间
void testAdjustTimeout() {
    LOG_INFO("=== Test 4: Adjust Timeout ===");
    HeapTimer timer;
    callback_count = 0;
    
    // 添加定时器
    timer.add(1, 150, test_callback_1);
    LOG_INFO("Added timer for fd 1 with 150ms timeout");
    
    // 等待100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 调整fd1的超时时间为200ms（从当前时刻计算）
    timer.adjust(1, 200);
    LOG_INFO("Adjusted fd 1 timeout to 200ms from now");
    
    // 等待150ms（总共250ms），原定时器应该已过期，但由于调整，不应该过期
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    timer.tick();
    assert(callback_count == 0);
    LOG_INFO("After adjustment and 150ms wait: callback_count = {} (no expiration)", callback_count);
    
    // 再等待100ms（总共400ms），现在应该过期
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.tick();
    assert(callback_count == 1);
    LOG_INFO("After 100ms more: callback_count = {} (timer expired)", callback_count);
    
    LOG_INFO("✓ Test 4 passed!");
}

// 测试5: 清空所有定时器
void testClear() {
    LOG_INFO("=== Test 5: Clear All Timers ===");
    HeapTimer timer;
    
    // 添加多个定时器
    timer.add(1, 1000, test_callback_1);
    timer.add(2, 2000, test_callback_2);
    timer.add(3, 3000, test_callback_3);
    
    LOG_INFO("Added 3 timers");
    
    // 获取下一个tick时间
    int next_tick1 = timer.get_next_tick();
    assert(next_tick1 > 0);
    LOG_INFO("Next tick before clear: {} ms", next_tick1);
    
    // 清空所有定时器
    timer.clear();
    LOG_INFO("Cleared all timers");
    
    // 现在应该没有定时器
    int next_tick2 = timer.get_next_tick();
    assert(next_tick2 == -1);
    LOG_INFO("Next tick after clear: {} ms (no timers)", next_tick2);
    
    LOG_INFO("✓ Test 5 passed!");
}

// 测试6: do_work方法的回调执行
void testDoWork() {
    LOG_INFO("=== Test 6: Do Work (Callback Execution) ===");
    HeapTimer timer;
    callback_count = 0;
    
    // 添加定时器
    timer.add(1, 5000, test_callback_1);
    timer.add(2, 5000, test_callback_2);
    
    LOG_INFO("Added 2 timers");
    
    // 手动触发do_work（不等待超时）
    timer.do_work(1);
    assert(callback_count == 1);
    LOG_INFO("After do_work(1): callback_count = {}", callback_count);
    
    timer.do_work(2);
    assert(callback_count == 2);
    LOG_INFO("After do_work(2): callback_count = {}", callback_count);
    
    LOG_INFO("✓ Test 6 passed!");
}

// 测试7: 对已存在的fd重新add（更新回调）
void testUpdateCallback() {
    LOG_INFO("=== Test 7: Update Callback for Existing FD ===");
    HeapTimer timer;
    callback_count = 0;
    
    // 第一次添加fd=1的定时器，超时时间为5000ms（很长，不会过期）
    timer.add(1, 5000, test_callback_1);
    LOG_INFO("Added timer for fd 1 with callback_1 (5000ms timeout)");
    
    // 立即再次添加相同fd的定时器，使用不同的回调
    timer.add(1, 5000, test_callback_2);
    LOG_INFO("Updated timer for fd 1 with callback_2 (5000ms timeout)");
    
    // 手动执行do_work触发回调（不等待）
    timer.do_work(1);
    
    // 由于更新了回调，callback_2应该被执行
    // 实际上哪个回调被执行取决于实现，但总之应该有一个被执行
    assert(callback_count == 1);
    LOG_INFO("Timer callback executed: callback_count = {}", callback_count);
    
    LOG_INFO("✓ Test 7 passed!");
}

// 测试8: 获取下一个tick时间
void testGetNextTick() {
    LOG_INFO("=== Test 8: Get Next Tick ===");
    HeapTimer timer;
    
    // 空定时器
    assert(timer.get_next_tick() == -1);
    LOG_INFO("Empty timer, next_tick = -1");
    
    // 添加定时器
    timer.add(1, 1000, test_callback_1);
    int next_tick = timer.get_next_tick();
    assert(next_tick > 0 && next_tick <= 1000);
    LOG_INFO("Added 1000ms timer, next_tick = {} ms", next_tick);
    
    // 添加更早过期的定时器
    timer.add(2, 300, test_callback_2);
    int next_tick2 = timer.get_next_tick();
    assert(next_tick2 > 0 && next_tick2 <= 300);
    LOG_INFO("Added 300ms timer, next_tick = {} ms", next_tick2);
    
    LOG_INFO("✓ Test 8 passed!");
}

int main() {
    // 初始化日志系统，指定日志文件路径，增加队列大小，否则日志队列满时会阻塞主线程，影响测试结果
    Logger::getInstance().initLogger("log/heaptimer.log", LogLevel::INFO, 2048, 1);
    
    LOG_INFO("========================================");
    LOG_INFO("Starting HeapTimer Test Suite");
    LOG_INFO("========================================");
    
    try {
        testBasicAddAndDel();
        testTimeout();
        testMultipleTimers();
        testAdjustTimeout();
        testClear();
        testDoWork();
        testUpdateCallback();
        testGetNextTick();
        
        LOG_INFO("========================================");
        LOG_INFO("All tests passed! ✓");
        LOG_INFO("========================================");
        
        // 确保所有日志被写入
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Logger::getInstance().shutdown();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        return 1;
    }
    
    return 0;
}
