#pragma once

/**
 * @file LogMacros.h
 * @brief 便利的日志宏定义接口
 *
 * 提供简洁易用的日志宏，自动包含文件名、行号、函数名等信息
 */

#include "Logger.h"
#include <sstream>

// ========== 基础日志宏 ==========

/**
 * @brief Trace级别日志宏
 * 自动包含文件名、行号、函数名
 */
#define LOG_TRACE(...) \
    mcpserver::Logger::getInstance().trace(__VA_ARGS__)

/**
 * @brief Debug级别日志宏
 * 自动包含文件名、行号、函数名
 */
#define LOG_DEBUG(...) \
    mcpserver::Logger::getInstance().debug(__VA_ARGS__)

/**
 * @brief Info级别日志宏
 * 自动包含文件名、行号、函数名
 */
#define LOG_INFO(...) \
    mcpserver::Logger::getInstance().info(__VA_ARGS__)

/**
 * @brief Warning级别日志宏
 * 自动包含文件名、行号、函数名
 */
#define LOG_WARN(...) \
    mcpserver::Logger::getInstance().warn(__VA_ARGS__)

/**
 * @brief Error级别日志宏
 * 自动包含文件名、行号、函数名
 */
#define LOG_ERROR(...) \
    mcpserver::Logger::getInstance().error(__VA_ARGS__)

/**
 * @brief Critical级别日志宏
 * 自动包含文件名、行号、函数名
 */
#define LOG_CRITICAL(...) \
    mcpserver::Logger::getInstance().critical(__VA_ARGS__)

// ========== 带上下文的日志宏 ==========

/**
 * @brief Trace级别日志宏（带函数上下文）
 */
#define LOG_TRACE_CTX(fmt, ...) \
    LOG_TRACE("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/**
 * @brief Debug级别日志宏（带函数上下文）
 */
#define LOG_DEBUG_CTX(fmt, ...) \
    LOG_DEBUG("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/**
 * @brief Info级别日志宏（带函数上下文）
 */
#define LOG_INFO_CTX(fmt, ...) \
    LOG_INFO("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/**
 * @brief Warning级别日志宏（带函数上下文）
 */
#define LOG_WARN_CTX(fmt, ...) \
    LOG_WARN("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/**
 * @brief Error级别日志宏（带函数上下文）
 */
#define LOG_ERROR_CTX(fmt, ...) \
    LOG_ERROR("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/**
 * @brief Critical级别日志宏（带函数上下文）
 */
#define LOG_CRITICAL_CTX(fmt, ...) \
    LOG_CRITICAL("[{}:{}] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

// ========== 条件日志宏 ==========

/**
 * @brief 条件Trace日志
 */
#define LOG_TRACE_IF(condition, ...) \
    do { if (condition) { LOG_TRACE(__VA_ARGS__); } } while(0)

/**
 * @brief 条件Debug日志
 */
#define LOG_DEBUG_IF(condition, ...) \
    do { if (condition) { LOG_DEBUG(__VA_ARGS__); } } while(0)

/**
 * @brief 条件Info日志
 */
#define LOG_INFO_IF(condition, ...) \
    do { if (condition) { LOG_INFO(__VA_ARGS__); } } while(0)

/**
 * @brief 条件Warning日志
 */
#define LOG_WARN_IF(condition, ...) \
    do { if (condition) { LOG_WARN(__VA_ARGS__); } } while(0)

/**
 * @brief 条件Error日志
 */
#define LOG_ERROR_IF(condition, ...) \
    do { if (condition) { LOG_ERROR(__VA_ARGS__); } } while(0)

/**
 * @brief 条件Critical日志
 */
#define LOG_CRITICAL_IF(condition, ...) \
    do { if (condition) { LOG_CRITICAL(__VA_ARGS__); } } while(0)

// ========== 调试/开发专用宏 ==========

#ifndef NDEBUG
    // Debug模式下启用的日志
    #define LOG_TRACE_DEBUG(...) LOG_TRACE(__VA_ARGS__)
    #define LOG_DEBUG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
#else
    // Release模式下禁用的日志
    #define LOG_TRACE_DEBUG(...) ((void)0)
    #define LOG_DEBUG_DEBUG(...) ((void)0)
#endif

// ========== 性能/作用域日志宏 ==========

/**
 * @brief 函数入口日志宏
 * 使用方法：在函数开头添加 LOG_FUNCTION_ENTRY();
 */
#define LOG_FUNCTION_ENTRY() \
    LOG_DEBUG(">>> Enter function: {}", __FUNCTION__)

/**
 * @brief 函数出口日志宏
 * 使用方法：在函数返回前添加 LOG_FUNCTION_EXIT();
 */
#define LOG_FUNCTION_EXIT() \
    LOG_DEBUG("<<< Exit function: {}", __FUNCTION__)

/**
 * @brief RAII风格的作用域日志
 * 在构造时记录进入，析构时记录退出
 */
class ScopeLog {
public:
    explicit ScopeLog(const char* func, const char* file = __FILE__, int line = __LINE__)
        : func_(func), file_(file), line_(line) {
        LOG_DEBUG("[{}] >>> Enter", func_);
    }

    ~ScopeLog() {
        LOG_DEBUG("[{}] <<< Exit", func_);
    }

private:
    const char* func_;
    const char* file_;
    int line_;
};

/**
 * @brief 作用域日志宏
 * 使用方法：在函数开头添加 SCOPE_LOG;
 */
#define SCOPE_LOG ScopeLog _scope_log_##__LINE__(__FUNCTION__)

// ========== 异常相关日志宏 ==========

/**
 * @brief 记录异常信息
 */
#define LOG_EXCEPTION(ex) \
    LOG_ERROR("Exception caught: {} at {}: {}", ex.what(), __FILE__, __LINE__)

/**
 * @brief 记录异常信息并返回
 */
#define LOG_EXCEPTION_RETURN(ex, ret_val) \
    do { \
        LOG_ERROR("Exception caught: {} at {}: {}", ex.what(), __FILE__, __LINE__); \
        return (ret_val); \
    } while(0)

// ========== 日志级别控制宏 ==========

/**
 * @brief 设置日志级别
 */
#define SET_LOG_LEVEL(level) \
    mcpserver::Logger::getInstance().setLogLevel(level)

/**
 * @brief 获取日志级别
 */
#define GET_LOG_LEVEL() \
    mcpserver::Logger::getInstance().getLogLevel()

/**
 * @brief 刷新日志缓冲区
 */
#define LOG_FLUSH() \
    mcpserver::Logger::getInstance().flush()

// ========== 日志系统控制宏 ==========

/**
 * @brief 初始化日志系统（使用Config）
 */
#define LOGGER_INIT_FROM_CONFIG() \
    do { \
        auto& cfg = mcpserver::Config::getInstance(); \
        mcpserver::Logger::getInstance().initialize( \
            cfg.getLogFilePath(), \
            cfg.getLogLevel(), \
            static_cast<size_t>(cfg.getLogFileSize()), \
            static_cast<size_t>(cfg.getLogFileCount()), \
            cfg.getLogConsoleOutput() \
        ); \
    } while(0)

/**
 * @brief 初始化日志系统（自定义参数）
 */
#define LOGGER_INIT(filepath, level, max_size, max_files, console) \
    mcpserver::Logger::getInstance().initialize( \
        filepath, level, static_cast<size_t>(max_size), \
        static_cast<size_t>(max_files), console \
    )

/**
 * @brief 关闭日志系统
 */
#define LOGGER_SHUTDOWN() \
    mcpserver::Logger::getInstance().shutdown()