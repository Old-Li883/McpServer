/**
 * @file test_logger.cpp
 * @brief 日志系统单元测试
 */

#include "gtest/gtest.h"
#include "../src/logger/Logger.h"
#include "../src/logger/LogMacros.h"
#include <thread>
#include <vector>
#include <chrono>

using namespace mcpserver;

// ========== 基础日志测试 ==========

TEST(LoggerTest, BasicInitialization) {
    EXPECT_NO_THROW({
        Logger::getInstance().initialize(
            "logs/test_basic.log",
            "debug",
            1024 * 1024 * 5,
            3,
            false  // 禁用控制台输出以避免测试输出混乱
        );
    });
}

TEST(LoggerTest, BasicLogging) {
    Logger::getInstance().initialize(
        "logs/test_basic_logging.log",
        "debug",
        1024 * 1024 * 5,
        3,
        false
    );

    EXPECT_NO_THROW({
        LOG_TRACE("This is a trace message");
        LOG_DEBUG("This is a debug message");
        LOG_INFO("This is an info message");
        LOG_WARN("This is a warning message");
        LOG_ERROR("This is an error message: code={}", 404);
        LOG_CRITICAL("This is a critical message!");
    });

    LOG_FLUSH();
}

TEST(LoggerTest, FormattedLogging) {
    Logger::getInstance().initialize(
        "logs/test_formatted.log",
        "debug",
        1024 * 1024 * 5,
        3,
        false
    );

    EXPECT_NO_THROW({
        LOG_INFO("Processing request: method={}, path={}", "GET", "/api/v1/users");
        LOG_WARN("High memory usage: {}%", 85.5);
    });

    LOG_FLUSH();
}

// ========== 条件日志测试 ==========

TEST(LoggerTest, ConditionalLogging) {
    Logger::getInstance().initialize(
        "logs/test_conditional.log",
        "debug",
        1024 * 1024 * 5,
        3,
        false
    );

    int value = 100;
    int threshold = 50;

    EXPECT_NO_THROW({
        LOG_INFO_IF(value > threshold, "Value {} exceeds threshold {}", value, threshold);
    });

    value = 30;
    EXPECT_NO_THROW({
        LOG_INFO_IF(value > threshold, "This should not be logged");
    });

    LOG_FLUSH();
}

// ========== 异常处理测试 ==========

TEST(LoggerTest, ExceptionLogging) {
    Logger::getInstance().initialize(
        "logs/test_exception.log",
        "debug",
        1024 * 1024 * 5,
        3,
        false
    );

    EXPECT_NO_THROW({
        try {
            throw std::runtime_error("Something went wrong!");
        } catch (const std::exception& ex) {
            LOG_EXCEPTION(ex);
        }
    });

    LOG_FLUSH();
}

// ========== 线程安全测试 ==========

TEST(LoggerTest, ThreadSafety) {
    Logger::getInstance().initialize(
        "logs/test_thread_safety.log",
        "info",
        1024 * 1024 * 5,
        3,
        false
    );

    auto threadWorker = [](int threadId) {
        for (int i = 0; i < 5; ++i) {
            LOG_INFO("Thread {}: iteration {}", threadId, i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    std::vector<std::thread> threads;
    EXPECT_NO_THROW({
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back(threadWorker, i);
        }

        for (auto& t : threads) {
            t.join();
        }
    });

    LOG_FLUSH();
}

// ========== 动态日志级别测试 ==========

TEST(LoggerTest, DynamicLogLevel) {
    Logger::getInstance().initialize(
        "logs/test_dynamic_level.log",
        "info",
        1024 * 1024 * 5,
        3,
        false
    );

    // 设置为TRACE级别，所有日志都会输出
    SET_LOG_LEVEL("trace");
    EXPECT_NO_THROW({
        LOG_TRACE("TRACE: This should be visible");
        LOG_DEBUG("DEBUG: This should be visible");
    });

    // 设置为WARN级别，只有WARN及以上会输出
    SET_LOG_LEVEL("warn");
    EXPECT_NO_THROW({
        LOG_TRACE("TRACE: This should NOT be visible");
        LOG_INFO("INFO: This should NOT be visible");
        LOG_WARN("WARN: This should be visible");
        LOG_ERROR("ERROR: This should be visible");
    });

    // 恢复为INFO级别
    SET_LOG_LEVEL("info");

    LOG_FLUSH();
}

// ========== 日志轮转测试 ==========

TEST(LoggerTest, LogRotation) {
    // 创建小文件测试轮转
    Logger::getInstance().initialize(
        "logs/test_rotation.log",
        "info",
        1024,  // 1KB 触发轮转
        3,     // 保留3个文件
        false
    );

    EXPECT_NO_THROW({
        // 生成大量日志以触发轮转
        for (int i = 0; i < 100; ++i) {
            LOG_INFO("Log rotation test message {}: This is a test message to fill up the log file and trigger rotation. "
                     "Lorem ipsum dolor sit amet, consectetur adipiscing elit.", i);
        }
    });

    LOG_FLUSH();
}

// ========== Scope Log测试 ==========

TEST(LoggerTest, ScopeLog) {
    Logger::getInstance().initialize(
        "logs/test_scope.log",
        "info",
        1024 * 1024 * 5,
        3,
        false
    );

    EXPECT_NO_THROW({
        SCOPE_LOG;
        LOG_INFO("Inside scoped function");
    });

    LOG_FLUSH();
}

// ========== Flush测试 ==========

TEST(LoggerTest, Flush) {
    Logger::getInstance().initialize(
        "logs/test_flush.log",
        "debug",
        1024 * 1024 * 5,
        3,
        false
    );

    EXPECT_NO_THROW({
        LOG_INFO("Before flush");
        LOG_FLUSH();
        LOG_INFO("After flush");
        LOG_FLUSH();
    });
}

// ========== 多次初始化测试 ==========

TEST(LoggerTest, MultipleInitialization) {
    // 第一次初始化
    EXPECT_NO_THROW({
        Logger::getInstance().initialize(
            "logs/test_multi_1.log",
            "debug",
            1024 * 1024 * 5,
            3,
            false
        );
        LOG_INFO("First initialization");
        LOG_FLUSH();
    });

    // 第二次初始化（应该正常工作）
    EXPECT_NO_THROW({
        Logger::getInstance().initialize(
            "logs/test_multi_2.log",
            "info",
            1024 * 1024 * 5,
            3,
            false
        );
        LOG_INFO("Second initialization");
        LOG_FLUSH();
    });
}

// ========== 性能测试 ==========

TEST(LoggerTest, PerformanceTest) {
    Logger::getInstance().initialize(
        "logs/test_performance.log",
        "info",
        1024 * 1024 * 5,
        3,
        false
    );

    auto start = std::chrono::high_resolution_clock::now();

    EXPECT_NO_THROW({
        for (int i = 0; i < 1000; ++i) {
            LOG_INFO("Performance test message {}", i);
        }
        LOG_FLUSH();
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 性能测试应该能在合理时间内完成
    EXPECT_LT(duration.count(), 5000);  // 5秒内完成
}
