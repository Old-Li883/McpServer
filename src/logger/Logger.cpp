#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/common.h>
#include <spdlog/cfg/env.h>

namespace mcpserver {

namespace {
    // Meyer's Singleton实现，确保线程安全
    Logger& getSingletonInstance() {
        static Logger instance;
        return instance;
    }
}

Logger& Logger::getInstance() {
    return getSingletonInstance();
}

Logger::~Logger() {
    // 简单释放资源，避免在程序退出时的析构顺序问题
    if (logger_) {
        logger_.reset();
        fileSink_.reset();
        consoleSink_.reset();
    }
}

bool Logger::initialize(const std::string& logFilePath,
                        const std::string& logLevel,
                        size_t maxFileSize,
                        size_t maxFiles,
                        bool enableConsole) {
    if (initialized_) {
        warn("Logger already initialized, skipping...");
        return true;
    }

    try {
        // 创建日志目录
        if (!createLogDirectory(logFilePath)) {
            std::cerr << "Failed to create log directory for: " << logFilePath << std::endl;
            return false;
        }

        // 创建日志 sinks
        std::vector<spdlog::sink_ptr> sinks;

        // 文件Sink - 使用rotating_file_sink_mt实现日志轮转（线程安全版本）
        fileSink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFilePath, maxFileSize, maxFiles);
        fileSink_->set_level(spdlog::level::trace);
        fileSink_->set_pattern(DEFAULT_FILE_PATTERN);
        sinks.push_back(fileSink_);

        // 控制台Sink - 使用颜色输出
        if (enableConsole) {
            consoleSink_ = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink_->set_level(spdlog::level::trace);
            consoleSink_->set_pattern(DEFAULT_CONSOLE_PATTERN);
            sinks.push_back(consoleSink_);
        }

        // 创建logger，使用多sink组合
        logger_ = std::make_shared<spdlog::logger>("mcpserver",
            sinks.begin(), sinks.end());

        // 设置默认日志级别
        logger_->set_level(stringToSpdlogLevel(logLevel));

        // 设置刷新级别（error及以上级别立即刷新）
        logger_->flush_on(spdlog::level::err);

        // 注册到spdlog全局注册表（可选，用于全局访问）
        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);

        // 设置错误处理回调
        spdlog::set_error_handler([](const std::string& msg) {
            std::cerr << "spdlog error: " << msg << std::endl;
        });

        initialized_ = true;

        info("Logger initialized successfully - File: {}, Level: {}, MaxSize: {}MB, MaxFiles: {}",
             logFilePath, logLevel, maxFileSize / (1024 * 1024), maxFiles);

        return true;

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return false;
    } catch (const std::exception& ex) {
        std::cerr << "Unexpected error during logger initialization: " << ex.what() << std::endl;
        return false;
    }
}

void Logger::setLogLevel(LogLevel level) {
    if (logger_) {
        logger_->set_level(toSpdlogLevel(level));
    }
}

void Logger::setLogLevel(const std::string& levelStr) {
    if (logger_) {
        logger_->set_level(stringToSpdlogLevel(levelStr));
    }
}

LogLevel Logger::getLogLevel() const {
    if (logger_) {
        switch (logger_->level()) {
            case spdlog::level::trace:    return LogLevel::TRACE;
            case spdlog::level::debug:    return LogLevel::DEBUG;
            case spdlog::level::info:     return LogLevel::INFO;
            case spdlog::level::warn:     return LogLevel::WARN;
            case spdlog::level::err:      return LogLevel::ERROR;
            case spdlog::level::critical: return LogLevel::CRITICAL;
            case spdlog::level::off:      return LogLevel::OFF;
            default:                      return LogLevel::INFO;
        }
    }
    return LogLevel::INFO;
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

void Logger::shutdown() {
    if (initialized_ && logger_) {
        logger_->flush();
        logger_.reset();
        fileSink_.reset();
        consoleSink_.reset();
        initialized_ = false;
    }
}

void Logger::setConsolePattern(const std::string& pattern) {
    if (consoleSink_) {
        consoleSink_->set_pattern(pattern);
    }
}

void Logger::setFilePattern(const std::string& pattern) {
    if (fileSink_) {
        fileSink_->set_pattern(pattern);
    }
}

LogLevel Logger::stringToLogLevel(const std::string& levelStr) {
    std::string lower = levelStr;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "trace")    return LogLevel::TRACE;
    if (lower == "debug")    return LogLevel::DEBUG;
    if (lower == "info")     return LogLevel::INFO;
    if (lower == "warn" || lower == "warning") return LogLevel::WARN;
    if (lower == "error")    return LogLevel::ERROR;
    if (lower == "critical") return LogLevel::CRITICAL;
    if (lower == "off")      return LogLevel::OFF;

    return LogLevel::INFO;  // 默认级别
}

spdlog::level::level_enum Logger::toSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:    return spdlog::level::trace;
        case LogLevel::DEBUG:    return spdlog::level::debug;
        case LogLevel::INFO:     return spdlog::level::info;
        case LogLevel::WARN:     return spdlog::level::warn;
        case LogLevel::ERROR:    return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF:      return spdlog::level::off;
        default:                 return spdlog::level::info;
    }
}

spdlog::level::level_enum Logger::stringToSpdlogLevel(const std::string& levelStr) {
    return toSpdlogLevel(stringToLogLevel(levelStr));
}

bool Logger::createLogDirectory(const std::string& logFilePath) {
    try {
        std::filesystem::path path(logFilePath);
        std::filesystem::path parentDir = path.parent_path();

        if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
            return std::filesystem::create_directories(parentDir);
        }
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "Failed to create log directory: " << ex.what() << std::endl;
        return false;
    }
}

} // namespace mcpserver