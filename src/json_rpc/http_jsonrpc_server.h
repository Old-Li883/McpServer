#pragma once

#include "jsonrpc.h"
#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace mcpserver::json_rpc {

/**
 * @brief SSE (Server-Sent Events) 事件数据
 */
struct SseEvent {
    std::string data;       // 事件数据
    std::string event;      // 事件类型（可选）
    std::string id;         // 事件ID（可选）
    int retry;              // 重连时间（毫秒，0表示不设置）

    SseEvent() : retry(0) {}
    SseEvent(std::string data) : data(std::move(data)), retry(0) {}
};

/**
 * @brief SSE 事件回调函数类型
 *
 * @param endpoint 端点路径
 * @param clientId 客户端ID（用于识别不同的连接）
 * @param event SSE 事件
 */
using SseEventCallback = std::function<void(
    const std::string& endpoint,
    const std::string& clientId,
    const SseEvent& event
)>;

/**
 * @brief SSE 连接状态回调
 *
 * @param endpoint 端点路径
 * @param clientId 客户端ID
 * @param connected true表示连接，false表示断开
 */
using SseConnectionCallback = std::function<void(
    const std::string& endpoint,
    const std::string& clientId,
    bool connected
)>;

/**
 * @brief HTTP JSON-RPC 服务器配置
 */
struct HttpJsonRpcServerConfig {
    std::string host = "0.0.0.0";       // 监听地址
    int port = 8080;                     // 监听端口
    int threadCount = 4;                 // 工作线程数（0表示自动）
    size_t maxPayloadSize = 64 * 1024 * 1024;  // 最大请求体大小
    size_t maxConnections = 1000;        // 最大并发连接数
    bool enableCors = false;             // 启用CORS
    std::string corsOrigin = "*";        // CORS允许的来源
    bool enableSse = true;               // 启用SSE支持
    int heartbeatInterval = 30;          // SSE心跳间隔（秒）
    int baseThreads = 2;                 // 基础IO线程数
};

/**
 * @brief HTTP JSON-RPC 服务器
 *
 * 基于 cpp-httplib 的 HTTP JSON-RPC 服务器实现。
 * 使用 Pimpl 模式隐藏实现细节。
 *
 * 功能特性:
 * - HTTP POST 接口处理 JSON-RPC 请求
 * - SSE (Server-Sent Events) 支持实时推送
 * - 线程安全的请求处理
 * - 配置化的服务器参数
 * - 优雅的启动和停止
 * - CORS 支持
 * - 连接管理和心跳检测
 */
class HttpJsonRpcServer {
public:
    /**
     * @brief 构造函数
     * @param rpc JSON-RPC 处理器
     * @param config 配置选项
     */
    explicit HttpJsonRpcServer(JsonRpc& rpc, const HttpJsonRpcServerConfig& config = HttpJsonRpcServerConfig());

    /**
     * @brief 析构函数
     */
    ~HttpJsonRpcServer();

    // 禁止拷贝和移动
    HttpJsonRpcServer(const HttpJsonRpcServer&) = delete;
    HttpJsonRpcServer& operator=(const HttpJsonRpcServer&) = delete;
    HttpJsonRpcServer(HttpJsonRpcServer&&) = delete;
    HttpJsonRpcServer& operator=(HttpJsonRpcServer&&) = delete;

    // ========== 服务器控制 ==========

    /**
     * @brief 启动服务器
     *
     * 阻塞当前线程，直到服务器停止。
     *
     * @return 是否成功启动
     */
    [[nodiscard]] bool start();

    /**
     * @brief 启动服务器（异步模式）
     *
     * 在后台线程中运行服务器。
     *
     * @return 是否成功启动
     */
    [[nodiscard]] bool startAsync();

    /**
     * @brief 停止服务器
     *
     * 优雅关闭，等待现有请求处理完成。
     */
    void stop();

    /**
     * @brief 等待服务器线程结束
     *
     * 仅在异步模式下有效。
     */
    void wait();

    /**
     * @brief 检查服务器是否正在运行
     * @return 是否正在运行
     */
    [[nodiscard]] bool isRunning() const;

    // ========== SSE 端点管理 ==========

    /**
     * @brief 注册 SSE 端点
     *
     * 注册一个 SSE 端点，客户端可以通过该端点订阅服务端推送事件。
     *
     * @param endpoint 端点路径（如 "/events"）
     * @param eventCallback 事件回调函数（当有事件需要发送时调用）
     * @param connectionCallback 连接状态回调（可选）
     * @return 是否注册成功
     */
    [[nodiscard]] bool registerSseEndpoint(
        const std::string& endpoint,
        SseEventCallback eventCallback,
        SseConnectionCallback connectionCallback = nullptr
    );

    /**
     * @brief 注销 SSE 端点
     * @param endpoint 端点路径
     * @return 是否注销成功
     */
    [[nodiscard]] bool unregisterSseEndpoint(const std::string& endpoint);

    /**
     * @brief 向指定端点的所有客户端发送 SSE 事件
     *
     * @param endpoint 端点路径
     * @param event SSE 事件
     * @return 发送的客户端数量
     */
    [[nodiscard]] size_t broadcastSseEvent(const std::string& endpoint, const SseEvent& event);

    /**
     * @brief 向指定端点的指定客户端发送 SSE 事件
     *
     * @param endpoint 端点路径
     * @param clientId 客户端ID
     * @param event SSE 事件
     * @return 是否发送成功
     */
    [[nodiscard]] bool sendSseEvent(const std::string& endpoint, const std::string& clientId, const SseEvent& event);

    /**
     * @brief 获取指定端点的连接客户端数量
     * @param endpoint 端点路径
     * @return 客户端数量
     */
    [[nodiscard]] size_t getSseClientCount(const std::string& endpoint) const;

    /**
     * @brief 获取所有已注册的 SSE 端点
     * @return 端点列表
     */
    [[nodiscard]] std::vector<std::string> getSseEndpoints() const;

    // ========== HTTP 路由管理 ==========

    /**
     * @brief 注册自定义 GET 处理器
     *
     * @param path 路径
     * @param handler 处理函数（参数：请求体，返回：响应体）
     * @return 是否注册成功
     */
    [[nodiscard]] bool registerGetHandler(
        const std::string& path,
        std::function<std::string(const std::string&)> handler
    );

    /**
     * @brief 注册自定义 POST 处理器
     *
     * @param path 路径
     * @param handler 处理函数（参数：请求体，返回：响应体）
     * @return 是否注册成功
     */
    [[nodiscard]] bool registerPostHandler(
        const std::string& path,
        std::function<std::string(const std::string&)> handler
    );

    /**
     * @brief 设置静态文件目录
     *
     * @param mountPoint 挂载点（如 "/public"）
     * @param directory 目录路径
     * @return 是否设置成功
     */
    [[nodiscard]] bool setStaticFileDir(const std::string& mountPoint, const std::string& directory);

    // ========== 状态查询 ==========

    /**
     * @brief 获取服务器地址
     * @return 地址字符串
     */
    [[nodiscard]] std::string getServerAddress() const;

    /**
     * @brief 获取服务器端口
     * @return 端口号
     */
    [[nodiscard]] int getServerPort() const;

    /**
     * @brief 获取已处理的请求数量
     * @return 请求数量
     */
    [[nodiscard]] size_t getProcessedRequestCount() const;

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
    void setConfig(const HttpJsonRpcServerConfig& config);

    /**
     * @brief 获取配置
     * @return 配置选项
     */
    [[nodiscard]] HttpJsonRpcServerConfig getConfig() const;

private:
    // Pimpl 实现类
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace mcpserver::json_rpc
