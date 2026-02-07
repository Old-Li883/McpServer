#include "Config.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

// 静态成员初始化
Config Config::instance_;

// 验证 JSON 配置的正确性
static bool validateConfig(const nlohmann::json& j) {
    // 有效的 server mode 值
    const std::set<std::string> validModes = {"both", "tcp", "stdio"};

    // 有效的 log level 值
    const std::set<std::string> validLogLevels = {"debug", "info", "warn", "error", "trace", "critical"};

    // 验证 server 配置
    if (j.contains("server")) {
        const auto& server = j["server"];

        // 验证 port
        if (server.contains("port")) {
            if (!server["port"].is_number_integer()) {
                return false;
            }
            int port = server["port"].get<int>();
            if (port < 1 || port > 65535) {
                return false;
            }
        }

        // 验证 mode
        if (server.contains("mode")) {
            if (!server["mode"].is_string()) {
                return false;
            }
            std::string mode = server["mode"].get<std::string>();
            if (validModes.find(mode) == validModes.end()) {
                return false;
            }
        }
    }

    // 验证 logging 配置
    if (j.contains("logging")) {
        const auto& logging = j["logging"];

        // 验证 log_file_path (可选，可以为空字符串)
        if (logging.contains("log_file_path")) {
            if (!logging["log_file_path"].is_string()) {
                return false;
            }
        }

        // 验证 log_level
        if (logging.contains("log_level")) {
            if (!logging["log_level"].is_string()) {
                return false;
            }
            std::string level = logging["log_level"].get<std::string>();
            if (validLogLevels.find(level) == validLogLevels.end()) {
                return false;
            }
        }

        // 验证 log_file_size
        if (logging.contains("log_file_size")) {
            if (!logging["log_file_size"].is_number_integer()) {
                return false;
            }
            if (logging["log_file_size"].get<int>() <= 0) {
                return false;
            }
        }

        // 验证 log_file_count
        if (logging.contains("log_file_count")) {
            if (!logging["log_file_count"].is_number_integer()) {
                return false;
            }
            if (logging["log_file_count"].get<int>() <= 0) {
                return false;
            }
        }

        // 验证 log_console_output
        if (logging.contains("log_console_output")) {
            if (!logging["log_console_output"].is_boolean()) {
                return false;
            }
        }
    }

    return true;
}

// 获取单例实例
Config& Config::getInstance() {
    return instance_;
}

// 私有构造函数 - 默认值
Config::Config()
    : serverPort_(8080)
    , serverMode_("both")
    , logFilePath_("")
    , logLevel_("info")
    , logFileSize_(111111)
    , logFileCount_(5)
    , logConsoleOutput_(true) {
}

// Server 配置 - Getter
int Config::getServerPort() const {
    return serverPort_;
}

std::string Config::getServerMode() const {
    return serverMode_;
}

// Server 配置 - Setter
void Config::setServerPort(int port) {
    serverPort_ = port;
}

void Config::setServerMode(const std::string& mode) {
    serverMode_ = mode;
}

// Logging 配置 - Getter
std::string Config::getLogFilePath() const {
    return logFilePath_;
}

std::string Config::getLogLevel() const {
    return logLevel_;
}

int Config::getLogFileSize() const {
    return logFileSize_;
}

int Config::getLogFileCount() const {
    return logFileCount_;
}

bool Config::getLogConsoleOutput() const {
    return logConsoleOutput_;
}

// Logging 配置 - Setter
void Config::setLogFilePath(const std::string& path) {
    logFilePath_ = path;
}

void Config::setLogLevel(const std::string& level) {
    logLevel_ = level;
}

void Config::setLogFileSize(int size) {
    logFileSize_ = size;
}

void Config::setLogFileCount(int count) {
    logFileCount_ = count;
}

void Config::setLogConsoleOutput(bool output) {
    logConsoleOutput_ = output;
}

// 从文件加载配置
bool Config::loadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j;
        file >> j;

        // 验证 JSON 配置
        if (!validateConfig(j)) {
            return false;
        }

        // 解析 server 配置
        if (j.contains("server")) {
            auto& server = j["server"];
            if (server.contains("port")) {
                serverPort_ = server["port"].get<int>();
            }
            if (server.contains("mode")) {
                serverMode_ = server["mode"].get<std::string>();
            }
        }

        // 解析 logging 配置
        if (j.contains("logging")) {
            auto& logging = j["logging"];
            if (logging.contains("log_file_path")) {
                logFilePath_ = logging["log_file_path"].get<std::string>();
            }
            if (logging.contains("log_level")) {
                logLevel_ = logging["log_level"].get<std::string>();
            }
            if (logging.contains("log_file_size")) {
                logFileSize_ = logging["log_file_size"].get<int>();
            }
            if (logging.contains("log_file_count")) {
                logFileCount_ = logging["log_file_count"].get<int>();
            }
            if (logging.contains("log_console_output")) {
                logConsoleOutput_ = logging["log_console_output"].get<bool>();
            }
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

// 保存配置到文件
bool Config::saveToFile(const std::string& filePath) {
    try {
        nlohmann::json j;

        // server 配置
        j["server"]["port"] = serverPort_;
        j["server"]["mode"] = serverMode_;

        // logging 配置
        j["logging"]["log_file_path"] = logFilePath_;
        j["logging"]["log_level"] = logLevel_;
        j["logging"]["log_file_size"] = logFileSize_;
        j["logging"]["log_file_count"] = logFileCount_;
        j["logging"]["log_console_output"] = logConsoleOutput_;

        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}