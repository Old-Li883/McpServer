#pragma once

#include <string>

class Config {
public:
    // 删除拷贝构造和赋值运算符
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // 获取单例实例
    static Config& getInstance();

    // Server 配置
    [[nodiscard]] int getServerPort() const;
    void setServerPort(int port);
    [[nodiscard]] std::string getServerMode() const;
    void setServerMode(const std::string& mode);

    // Logging 配置
    [[nodiscard]] std::string getLogFilePath() const;
    void setLogFilePath(const std::string& path);
    [[nodiscard]] std::string getLogLevel() const;
    void setLogLevel(const std::string& level);
    [[nodiscard]] int getLogFileSize() const;
    void setLogFileSize(int size);
    [[nodiscard]] int getLogFileCount() const;
    void setLogFileCount(int count);
    [[nodiscard]] bool getLogConsoleOutput() const;
    void setLogConsoleOutput(bool output);

    // 从文件加载配置
    bool loadFromFile(const std::string& filePath);

    // 保存配置到文件
    bool saveToFile(const std::string& filePath);

private:
    // 私有构造函数
    Config();
    ~Config() = default;

    // Server 配置
    int serverPort_;
    std::string serverMode_;

    // Logging 配置
    std::string logFilePath_;
    std::string logLevel_;
    int logFileSize_;
    int logFileCount_;
    bool logConsoleOutput_;

    // 静态实例
    static Config instance_;
};
