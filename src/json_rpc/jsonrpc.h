#pragma once

#include "jsonrpc_serialization.h"
#include <functional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>

namespace mcpserver::json_rpc {

/**
 * @brief JSON-RPC 方法处理器返回值
 */
using MethodResult = nlohmann::json;

/**
 * @brief JSON-RPC 方法处理器函数签名
 *
 * @param method 方法名
 * @param params 参数 (可以是 array 或 object)
 * @return 方法执行结果
 * @throws std::exception 方法执行异常将被转换为内部错误
 */
using MethodHandler = std::function<MethodResult(const std::string& method, const nlohmann::json& params)>;

/**
 * @brief 异步方法处理器（带回调）
 */
using AsyncMethodHandler = std::function<void(
    const std::string& method,
    const nlohmann::json& params,
    std::function<void(const MethodResult&)> callback,
    std::function<void(int, const std::string&)> errorCallback
)>;

/**
 * @brief JSON-RPC 2.0 处理器配置
 */
struct JsonRpcConfig {
    bool strictMode = true;          // 严格模式：验证所有请求
    bool allowNotification = true;   // 允许通知（无 ID 的请求）
    bool allowBatch = true;          // 允许批量请求
    int maxBatchSize = 100;          // 批量请求最大数量
};

/**
 * @brief JSON-RPC 2.0 核心处理器
 *
 * 功能特性:
 * - 方法注册与调用
 * - 请求/响应处理
 * - 批量请求支持
 * - 通知支持（无响应）
 * - 错误处理
 * - 线程安全
 * - 异步方法调用支持
 */
class JsonRpc {
public:
    /**
     * @brief 构造函数
     * @param config 配置选项
     */
    explicit JsonRpc(const JsonRpcConfig& config = JsonRpcConfig());

    /**
     * @brief 析构函数
     */
    ~JsonRpc();

    // 禁止拷贝和移动
    JsonRpc(const JsonRpc&) = delete;
    JsonRpc& operator=(const JsonRpc&) = delete;
    JsonRpc(JsonRpc&&) = delete;
    JsonRpc& operator=(JsonRpc&&) = delete;

    // ========== 方法注册 ==========

    /**
     * @brief 注册同步方法处理器
     * @param method 方法名
     * @param handler 方法处理器
     * @return 注册是否成功
     */
    bool registerMethod(const std::string& method, MethodHandler handler);

    /**
     * @brief 注册异步方法处理器
     * @param method 方法名
     * @param handler 异步方法处理器
     * @return 注册是否成功
     */
    bool registerAsyncMethod(const std::string& method, AsyncMethodHandler handler);

    /**
     * @brief 注销方法
     * @param method 方法名
     * @return 注销是否成功
     */
    bool unregisterMethod(const std::string& method);

    /**
     * @brief 检查方法是否已注册
     * @param method 方法名
     * @return 是否已注册
     */
    [[nodiscard]] bool hasMethod(const std::string& method) const;

    /**
     * @brief 获取所有已注册的方法名
     * @return 方法名列表
     */
    [[nodiscard]] std::vector<std::string> getRegisteredMethods() const;

    /**
     * @brief 清除所有已注册的方法
     */
    void clearMethods();

    // ========== 请求处理 ==========

    /**
     * @brief 处理单个请求
     * @param request 请求对象
     * @return 响应对象（通知时返回空）
     */
    [[nodiscard]] std::optional<Response> handleRequest(const Request& request);

    /**
     * @brief 处理批量请求
     * @param requests 批量请求对象
     * @return 批量响应对象（可能为空）
     */
    [[nodiscard]] std::optional<BatchResponse> handleBatchRequest(const BatchRequest& requests);

    /**
     * @brief 处理 JSON 字符串请求
     * @param jsonStr JSON 字符串
     * @return 响应 JSON 字符串（通知时返回空）
     * @throws SerializationException JSON 解析失败
     */
    [[nodiscard]] std::optional<std::string> handleRequest(const std::string& jsonStr);

    // ========== 配置管理 ==========

    /**
     * @brief 设置配置
     * @param config 配置选项
     */
    void setConfig(const JsonRpcConfig& config);

    /**
     * @brief 获取配置
     * @return 配置选项
     */
    [[nodiscard]] JsonRpcConfig getConfig() const;

    // ========== 错误回调 ==========

    /**
     * @brief 设置未捕获异常处理器
     *
     * 当方法处理器抛出异常时调用此回调
     *
     * @param handler 异常处理器回调
     */
    void setExceptionHandler(std::function<void(const std::exception&)> handler);

    /**
     * @brief 设置方法未找到处理器
     *
     * 当请求的方法不存在时调用此回调
     *
     * @param handler 方法未找到处理器回调
     */
    void setMethodNotFoundHandler(std::function<Response(const std::string&, const nlohmann::json&)> handler);

    // ========== 便捷方法 ==========

    /**
     * @brief 创建通知请求
     * @param method 方法名
     * @param params 参数
     * @return 请求对象
     */
    [[nodiscard]] static Request createNotification(const std::string& method,
                                                     const nlohmann::json& params = nullptr);

    /**
     * @brief 创建调用请求
     * @param method 方法名
     * @param params 参数
     * @param id 请求 ID
     * @return 请求对象
     */
    [[nodiscard]] static Request createRequest(const std::string& method,
                                                const nlohmann::json& params = nullptr,
                                                const nlohmann::json& id = 1);

    /**
     * @brief 创建带有字符串 ID 的调用请求
     */
    [[nodiscard]] static Request createRequest(const std::string& method,
                                                const nlohmann::json& params,
                                                const std::string& id);

private:
    /**
     * @brief 处理请求的核心逻辑
     */
    [[nodiscard]] Response processRequest(const Request& request);

    /**
     * @brief 验证请求
     */
    [[nodiscard]] bool validateRequest(const Request& request) const;

    /**
     * @brief 调用方法处理器
     */
    [[nodiscard]] std::optional<MethodResult> invokeMethod(const std::string& method,
                                                             const nlohmann::json& params);

    /**
     * @brief 创建标准错误响应
     */
    [[nodiscard]] Response createErrorResponse(ErrorCode code,
                                                const nlohmann::json& id,
                                                const std::string& detail = "") const;

private:
    struct MethodEntry {
        MethodHandler syncHandler;
        AsyncMethodHandler asyncHandler;
        bool isAsync;
    };

    mutable std::mutex mutex_;
    JsonRpcConfig config_;
    std::unordered_map<std::string, MethodEntry> methods_;

    // 回调函数
    std::function<void(const std::exception&)> exceptionHandler_;
    std::function<Response(const std::string&, const nlohmann::json&)> methodNotFoundHandler_;

    // 序列化工具
    JsonRpcSerialization serialization_;
};

} // namespace mcpserver::json_rpc
