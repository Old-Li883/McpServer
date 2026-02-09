#pragma once

#include "jsonrpc.h"
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace mcpserver::json_rpc {

/**
 * @brief 消息回调函数类型
 */
using MessageCallback = std::function<void(const std::string&)>;

/**
 * @brief Stdio JSON-RPC 配置
 */
struct StdioJsonRpcConfig {
    size_t maxBufferSize = 1024 * 1024;      // 最大缓冲区大小 (1MB)
    size_t maxMessageSize = 64 * 1024 * 1024; // 最大消息大小 (64MB)
    bool enableDebugLog = false;              // 启用调试日志
    bool useLspFormat = true;                 // 使用 LSP/MCP 格式 (Content-Length 头)
};

/**
 * @brief 标准输入输出 JSON-RPC 传输层
 *
 * 功能特性:
 * - 从标准输入读取 JSON-RPC 消息
 * - 向标准输出写入 JSON-RPC 响应
 * - 支持逐行读取和批量消息处理
 * - 异步读取支持
 * - 线程安全
 * - 错误处理和恢复
 */
class StdioJsonRpc {
public:
    /**
     * @brief 构造函数
     * @param rpc JSON-RPC 处理器
     * @param config 配置选项
     */
    explicit StdioJsonRpc(JsonRpc& rpc, const StdioJsonRpcConfig& config = StdioJsonRpcConfig());

    /**
     * @brief 析构函数
     */
    ~StdioJsonRpc();

    // 禁止拷贝和移动
    StdioJsonRpc(const StdioJsonRpc&) = delete;
    StdioJsonRpc& operator=(const StdioJsonRpc&) = delete;
    StdioJsonRpc(StdioJsonRpc&&) = delete;
    StdioJsonRpc& operator=(StdioJsonRpc&&) = delete;

    // ========== 消息接收回调 ==========

    /**
     * @brief 设置接收到请求时的回调
     * @param callback 回调函数
     */
    void setRequestCallback(MessageCallback callback);

    /**
     * @brief 设置接收到响应时的回调
     * @param callback 回调函数
     */
    void setResponseCallback(MessageCallback callback);

    /**
     * @brief 设置发生错误时的回调
     * @param callback 回调函数
     */
    void setErrorCallback(MessageCallback callback);

    // ========== 同步操作 ==========

    /**
     * @brief 读取单条消息并处理
     * @return 是否成功读取和处理
     */
    [[nodiscard]] bool readAndProcess();

    /**
     * @brief 发送响应消息到标准输出
     * @param message JSON 字符串消息
     * @return 是否成功发送
     */
    [[nodiscard]] bool sendMessage(const std::string& message);

    /**
     * @brief 发送请求到标准输出（等待响应）
     * @param message JSON 字符串消息
     * @return 响应消息
     */
    [[nodiscard]] std::optional<std::string> sendRequest(const std::string& message);

    // ========== 异步操作 ==========

    /**
     * @brief 启动异步读取循环
     * @return 是否成功启动
     */
    [[nodiscard]] bool start();

    /**
     * @brief 停止异步读取循环
     */
    void stop();

    /**
     * @brief 检查是否正在运行
     * @return 是否正在运行
     */
    [[nodiscard]] bool isRunning() const;

    // ========== 批量消息处理 ==========

    /**
     * @brief 处理所有可用的消息
     * @return 处理的消息数量
     */
    [[nodiscard]] int processAllAvailable();

    // ========== 状态查询 ==========

    /**
     * @brief 获取已接收的消息计数
     * @return 消息计数
     */
    [[nodiscard]] size_t getReceivedMessageCount() const;

    /**
     * @brief 获取已发送的消息计数
     * @return 消息计数
     */
    [[nodiscard]] size_t getSentMessageCount() const;

    /**
     * @brief 获取最后的错误信息
     * @return 错误信息
     */
    [[nodiscard]] std::string getLastError() const;

    // ========== 配置管理 ==========

    /**
     * @brief 设置配置
     * @param config 配置选项
     */
    void setConfig(const StdioJsonRpcConfig& config);

    /**
     * @brief 获取配置
     * @return 配置选项
     */
    [[nodiscard]] StdioJsonRpcConfig getConfig() const;

    // ========== 工具方法 ==========

    /**
     * @brief 清空输入缓冲区
     */
    void clearInputBuffer();

    /**
     * @brief 刷新输出缓冲区
     */
    void flushOutput();

private:
    /**
     * @brief 异步读取线程函数
     */
    void readThreadFunc();

    /**
     * @brief 读取单条消息（LSP/MCP 格式或简单行格式）
     * @return 消息字符串
     */
    [[nodiscard]] std::optional<std::string> readMessage();

    /**
     * @brief 读取 LSP/MCP 格式消息
     * @return 消息字符串
     */
    [[nodiscard]] std::optional<std::string> readLspMessage();

    /**
     * @brief 读取简单行格式消息
     * @return 消息字符串
     */
    [[nodiscard]] std::optional<std::string> readLineMessage();

    /**
     * @brief 读取一行
     * @return 行内容
     */
    [[nodiscard]] std::optional<std::string> readLine();

    /**
     * @brief 读取指定字节数
     * @param size 要读取的字节数
     * @return 读取的内容
     */
    [[nodiscard]] std::optional<std::string> readBytes(size_t size);

    /**
     * @brief 处理接收到的消息
     * @param message 消息字符串
     */
    void processMessage(const std::string& message);

    /**
     * @brief 检查消息大小是否超过限制
     * @param message 消息字符串
     * @return 是否超过限制
     */
    [[nodiscard]] bool isMessageTooLarge(const std::string& message) const;

    /**
     * @brief 设置最后的错误信息
     * @param error 错误信息
     */
    void setLastError(const std::string& error);

private:
    // JSON-RPC 处理器引用
    JsonRpc& rpc_;

    // 配置
    StdioJsonRpcConfig config_;

    // 异步读取
    std::unique_ptr<std::thread> readThread_;
    std::atomic<bool> running_;
    std::atomic<bool> shouldStop_;

    // 消息队列（用于异步读取）
    std::queue<std::string> messageQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    // 回调函数
    MessageCallback requestCallback_;
    MessageCallback responseCallback_;
    MessageCallback errorCallback_;

    // 统计信息
    std::atomic<size_t> receivedCount_;
    std::atomic<size_t> sentCount_;

    // 错误信息
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

} // namespace mcpserver::json_rpc
