#include "stdio_jsonrpc.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace mcpserver::json_rpc {

StdioJsonRpc::StdioJsonRpc(JsonRpc& rpc, const StdioJsonRpcConfig& config)
    : rpc_(rpc),
      config_(config),
      running_(false),
      shouldStop_(false),
      receivedCount_(0),
      sentCount_(0) {
}

StdioJsonRpc::~StdioJsonRpc() {
    stop();
}

// ========== 消息接收回调 ==========

void StdioJsonRpc::setRequestCallback(MessageCallback callback) {
    requestCallback_ = std::move(callback);
}

void StdioJsonRpc::setResponseCallback(MessageCallback callback) {
    responseCallback_ = std::move(callback);
}

void StdioJsonRpc::setErrorCallback(MessageCallback callback) {
    errorCallback_ = std::move(callback);
}

// ========== 同步操作 ==========

bool StdioJsonRpc::readAndProcess() {
    auto message = readLine();
    if (!message.has_value()) {
        return false;
    }

    processMessage(message.value());
    return true;
}

bool StdioJsonRpc::sendMessage(const std::string& message) {
    try {
        if (config_.enableDebugLog) {
            std::cerr << "[DEBUG] Sending message: " << message << std::endl;
        }

        std::cout << message << config_.delimiter;
        std::cout.flush();

        sentCount_++;
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to send message: " + std::string(e.what()));
        return false;
    }
}

std::optional<std::string> StdioJsonRpc::sendRequest(const std::string& message) {
    if (!sendMessage(message)) {
        return std::nullopt;
    }

    // 读取响应
    auto response = readLine();
    if (response.has_value()) {
        receivedCount_++;
    }

    return response;
}

// ========== 异步操作 ==========

bool StdioJsonRpc::start() {
    if (running_.load()) {
        return false;  // 已经在运行
    }

    shouldStop_.store(false);
    running_.store(true);

    try {
        readThread_ = std::make_unique<std::thread>(&StdioJsonRpc::readThreadFunc, this);
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to start read thread: " + std::string(e.what()));
        running_.store(false);
        return false;
    }
}

void StdioJsonRpc::stop() {
    if (!running_.load()) {
        return;
    }

    shouldStop_.store(true);

    // 通知读取线程
    queueCondition_.notify_all();

    if (readThread_ && readThread_->joinable()) {
        readThread_->join();
    }

    readThread_.reset();
    running_.store(false);
}

bool StdioJsonRpc::isRunning() const {
    return running_.load();
}

// ========== 批量消息处理 ==========

int StdioJsonRpc::processAllAvailable() {
    int count = 0;

    while (readAndProcess()) {
        count++;
    }

    return count;
}

// ========== 状态查询 ==========

size_t StdioJsonRpc::getReceivedMessageCount() const {
    return receivedCount_.load();
}

size_t StdioJsonRpc::getSentMessageCount() const {
    return sentCount_.load();
}

std::string StdioJsonRpc::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

// ========== 配置管理 ==========

void StdioJsonRpc::setConfig(const StdioJsonRpcConfig& config) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    config_ = config;
}

StdioJsonRpcConfig StdioJsonRpc::getConfig() const {
    return config_;
}

// ========== 工具方法 ==========

void StdioJsonRpc::clearInputBuffer() {
    // 设置为非阻塞模式并清空缓冲区
    std::cin.sync();
    std::cin.clear();

    // 读取并丢弃所有可用字符
    while (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.get();
    }
}

void StdioJsonRpc::flushOutput() {
    std::cout.flush();
    std::cerr.flush();
}

// ========== 私有方法 ==========

void StdioJsonRpc::readThreadFunc() {
    while (!shouldStop_.load()) {
        auto message = readLine();

        if (message.has_value()) {
            // 将消息添加到队列
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                messageQueue_.push(message.value());
            }
            queueCondition_.notify_one();

            // 处理消息
            processMessage(message.value());
            receivedCount_++;
        } else {
            // 读取失败或 EOF
            if (std::cin.eof()) {
                // EOF，退出循环
                break;
            }

            // 短暂休眠避免忙等待
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

std::optional<std::string> StdioJsonRpc::readLine() {
    try {
        std::string line;

        // 检查消息大小
        if (std::cin.peek() == std::char_traits<char>::eof()) {
            return std::nullopt;
        }

        // 读取一行
        if (!std::getline(std::cin, line)) {
            if (std::cin.eof()) {
                return std::nullopt;
            }
            setLastError("Failed to read from stdin");
            return std::nullopt;
        }

        // 检查是否为空行
        if (line.empty()) {
            return std::nullopt;
        }

        // 检查消息大小限制
        if (isMessageTooLarge(line)) {
            setLastError("Message size exceeds limit");
            return std::nullopt;
        }

        if (config_.enableDebugLog) {
            std::cerr << "[DEBUG] Received message: " << line << std::endl;
        }

        return line;
    } catch (const std::exception& e) {
        setLastError("Error reading line: " + std::string(e.what()));
        return std::nullopt;
    }
}

void StdioJsonRpc::processMessage(const std::string& message) {
    try {
        // 尝试将消息作为请求处理
        auto response = rpc_.handleRequest(message);

        if (response.has_value()) {
            // 有响应，发送回去
            [[maybe_unused]] bool sent = sendMessage(response.value());

            // 调用响应回调
            if (responseCallback_) {
                responseCallback_(response.value());
            }
        } else {
            // 通知，无响应
            // 调用请求回调（表示已处理通知）
            if (requestCallback_) {
                requestCallback_(message);
            }
        }
    } catch (const SerializationException& e) {
        std::string error = "Serialization error: " + std::string(e.what());
        setLastError(error);

        if (errorCallback_) {
            errorCallback_(error);
        }
    } catch (const std::exception& e) {
        std::string error = "Error processing message: " + std::string(e.what());
        setLastError(error);

        if (errorCallback_) {
            errorCallback_(error);
        }
    }
}

bool StdioJsonRpc::isMessageTooLarge(const std::string& message) const {
    return message.length() > config_.maxMessageSize;
}

void StdioJsonRpc::setLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;

    if (config_.enableDebugLog) {
        std::cerr << "[ERROR] " << error << std::endl;
    }
}

} // namespace mcpserver::json_rpc
