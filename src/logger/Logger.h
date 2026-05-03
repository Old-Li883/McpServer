#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace mcpserver {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

/**
 * @brief 日志系统单例类
 *
 * 功能特性：
 * - 单例模式：全局唯一Logger实例，线程安全
 * - Sink机制：支持多目标输出（控制台+文件）
 * - 日志轮转：自动按文件大小进行日志轮转
 * - 动态级别：支持运行时动态调整日志级别
 * - 线程安全：所有操作均为线程安全
 * - Pattern定制：不同sink使用不同的日志格式
 */
class Logger {
public:
    // 删除拷贝构造和赋值运算符
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief 构造函数（Meyer's Singleton需要）
     */
    Logger() = default;

    /**
     * @brief 析构函数
     */
    ~Logger();

    /**
     * @brief 获取单例实例
     * @return Logger实例的引用
     */
    static Logger& getInstance();

    /**
     * @brief 初始化日志系统
     * @param logFilePath 日志文件路径
     * @param logLevel 日志级别 ("trace", "debug", "info", "warn", "error", "critical")
     * @param maxFileSize 单个日志文件最大大小（字节）
     * @param maxFiles 保留的日志文件数量
     * @param enableConsole 是否启用控制台输出
     * @return 初始化是否成功
     */
    bool initialize(const std::string& logFilePath = "logs/mcpserver.log",
                   const std::string& logLevel = "info",
                   size_t maxFileSize = 1024 * 1024 * 5,  // 5MB
                   size_t maxFiles = 3,
                   bool enableConsole = true);

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief 设置日志级别（字符串）
     * @param levelStr 日志级别字符串 ("trace", "debug", "info", "warn", "error", "critical")
     */
    void setLogLevel(const std::string& levelStr);

    /**
     * @brief 获取当前日志级别
     * @return 当前日志级别
     */
    [[nodiscard]] LogLevel getLogLevel() const;

    /**
     * @brief 刷新日志缓冲区
     */
    void flush();

    /**
     * @brief 关闭日志系统
     */
    void shutdown();

    // ========== 日志记录接口 ==========

    template<typename... Args>
    void trace(const std::string& fmt, Args&&... args) {
        if (logger_) {
            logger_->trace(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args) {
        if (logger_) {
            logger_->debug(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void info(const std::string& fmt, Args&&... args) {
        if (logger_) {
            logger_->info(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args) {
        if (logger_) {
            logger_->warn(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void error(const std::string& fmt, Args&&... args) {
        if (logger_) {
            logger_->error(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void critical(const std::string& fmt, Args&&... args) {
        if (logger_) {
            logger_->critical(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief 获取原始spdlog logger（用于高级用法）
     * @return spdlog logger的共享指针
     */
    [[nodiscard]] std::shared_ptr<spdlog::logger> getRawLogger() const { return logger_; }

    /**
     * @brief 设置控制台输出Pattern
     * @param pattern 格式化字符串
     */
    void setConsolePattern(const std::string& pattern);

    /**
     * @brief 设置文件输出Pattern
     * @param pattern 格式化字符串
     */
    void setFilePattern(const std::string& pattern);

private:
    /**
     * @brief 将字符串转换为日志级别
     * @param levelStr 日志级别字符串
     * @return 日志级别
     */
    static LogLevel stringToLogLevel(const std::string& levelStr);

    /**
     * @brief 将日志级别转换为spdlog级别
     * @param level 日志级别
     * @return spdlog日志级别
     */
    static spdlog::level::level_enum toSpdlogLevel(LogLevel level);

    /**
     * @brief 将字符串转换为spdlog日志级别
     * @param levelStr 日志级别字符串
     * @return spdlog日志级别
     */
    static spdlog::level::level_enum stringToSpdlogLevel(const std::string& levelStr);

    /**
     * @brief 创建日志目录
     * @param logFilePath 日志文件路径
     * @return 是否创建成功
     */
    bool createLogDirectory(const std::string& logFilePath);

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> fileSink_;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> consoleSink_;
    bool initialized_ = false;

    // 默认Pattern格式
    static constexpr const char* DEFAULT_CONSOLE_PATTERN =
        "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v";
    static constexpr const char* DEFAULT_FILE_PATTERN =
        "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v";
};

} // namespace mcpserver