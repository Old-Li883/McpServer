/**
 * @file test_logger.cpp
 * @brief 日志系统使用示例和测试
 */

#include "../src/logger/Logger.h"
#include "../src/logger/LogMacros.h"
#include <thread>
#include <vector>
#include <chrono>

// 使用示例1：基本日志输出
void basicLoggingExample() {
    SCOPE_LOG;  // 自动记录函数进入和退出

    LOG_TRACE("This is a trace message");
    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARN("This is a warning message");
    LOG_ERROR("This is an error message: code={}", 404);
    LOG_CRITICAL("This is a critical message!");

    // 带格式化的日志
    LOG_INFO("Processing request: method={}, path={}", "GET", "/api/v1/users");
    LOG_WARN("High memory usage: {}%", 85.5);
}

// 使用示例2：条件日志
void conditionalLoggingExample() {
    int value = 100;
    int threshold = 50;

    LOG_INFO_IF(value > threshold, "Value {} exceeds threshold {}", value, threshold);

    // 调试模式下才会输出的日志
    LOG_DEBUG_DEBUG("This only appears in debug builds");
}

// 使用示例3：异常处理
void exceptionExample() {
    try {
        throw std::runtime_error("Something went wrong!");
    } catch (const std::exception& ex) {
        LOG_EXCEPTION(ex);
    }
}

// 使用示例4：线程安全测试
void threadWorker(int threadId) {
    for (int i = 0; i < 5; ++i) {
        LOG_INFO("Thread {}: iteration {}", threadId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void threadSafetyExample() {
    LOG_INFO("Starting thread safety test with 5 threads...");

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(threadWorker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    LOG_INFO("Thread safety test completed");
}

// 使用示例5：动态调整日志级别
void dynamicLogLevelExample() {
    LOG_INFO("=== Dynamic Log Level Test ===");

    // 设置为TRACE级别，所有日志都会输出
    SET_LOG_LEVEL("trace");
    LOG_TRACE("TRACE: This should be visible");
    LOG_DEBUG("DEBUG: This should be visible");

    // 设置为WARN级别，只有WARN及以上会输出
    SET_LOG_LEVEL("warn");
    LOG_TRACE("TRACE: This should NOT be visible");
    LOG_INFO("INFO: This should NOT be visible");
    LOG_WARN("WARN: This should be visible");
    LOG_ERROR("ERROR: This should be visible");

    // 恢复为INFO级别
    SET_LOG_LEVEL("info");
    LOG_INFO("=== Log level restored to INFO ===");
}

// 使用示例6：自定义Pattern
void customPatternExample() {
    LOG_INFO("=== Custom Pattern Test ===");

    // 控制台使用带颜色的格式
    mcpserver::Logger::getInstance().setConsolePattern(
        "[%H:%M:%S] [%^%l%$] %v"
    );

    // 文件使用更详细的格式（包含源文件和行号）
    mcpserver::Logger::getInstance().setFilePattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] [%!] %v"
    );

    LOG_INFO("This log uses custom patterns");
    LOG_WARN("Check the log file for detailed source information");
}

// 使用示例7：使用Config配置日志系统
// 注：此功能需要链接Config模块，在实际项目中可取消注释
/*
void configBasedInitialization() {
    // 从Config读取配置并初始化日志系统
    LOGGER_INIT_FROM_CONFIG();

    LOG_INFO("Logger initialized from Config");
    LOG_INFO("Log file: {}", Config::getInstance().getLogFilePath());
    LOG_INFO("Log level: {}", Config::getInstance().getLogLevel());
}
*/

// 使用示例8：日志轮转测试（需要大量日志才能触发）
void logRotationTest() {
    LOG_INFO("=== Log Rotation Test ===");
    LOG_INFO("Generating logs to test rotation...");

    // 生成大量日志以触发轮转（如果文件大小限制较小）
    for (int i = 0; i < 1000; ++i) {
        LOG_INFO("Log rotation test message {}: This is a test message to fill up the log file and trigger rotation. "
                 "Lorem ipsum dolor sit amet, consectetur adipiscing elit.", i);
    }

    LOG_INFO("Log rotation test completed. Check the logs directory for rotated files.");
}

int main() {
    // ========== 初始化方式1：使用默认配置 ==========
    // mcpserver::Logger::getInstance().initialize();

    // ========== 初始化方式2：自定义配置 ==========
    mcpserver::Logger::getInstance().initialize(
        "logs/mcpserver.log",    // 日志文件路径
        "debug",                  // 日志级别
        1024 * 1024 * 5,         // 单个文件最大5MB
        3,                        // 保留3个日志文件
        true                      // 启用控制台输出
    );

    // ========== 初始化方式3：从Config读取 ==========
    // configBasedInitialization(); // 需要链接Config模块

    // 设置控制台为带颜色的简洁格式
    mcpserver::Logger::getInstance().setConsolePattern(
        "%^[%Y-%m-%d %H:%M:%S.%e]%$ [%^%l%$] [%t] %v"
    );

    // 打印欢迎信息
    LOG_INFO("╔════════════════════════════════════════════════════════════╗");
    LOG_INFO("║          MCP Server Logging System Test Suite             ║");
    LOG_INFO("╚════════════════════════════════════════════════════════════╝");
    LOG_INFO("");

    // 运行各个示例
    basicLoggingExample();
    LOG_INFO("");

    conditionalLoggingExample();
    LOG_INFO("");

    dynamicLogLevelExample();
    LOG_INFO("");

    customPatternExample();
    LOG_INFO("");

    exceptionExample();
    LOG_INFO("");

    threadSafetyExample();
    LOG_INFO("");

    // 如果需要测试日志轮转，取消下面的注释
    // logRotationTest();

    // 程序结束前刷新日志
    LOG_FLUSH();

    // 关闭日志系统（可选，析构函数会自动处理）
    // LOGGER_SHUTDOWN();

    return 0;
}