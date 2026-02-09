#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace mcpserver::json_rpc {

/**
 * @brief JSON-RPC 2.0 错误码定义
 *
 * 参考 JSON-RPC 2.0 规范: https://www.jsonrpc.org/specification
 */
enum class ErrorCode {
    // 标准错误码
    PARSE_ERROR = -32700,           // 解析错误
    INVALID_REQUEST = -32600,       // 无效请求
    METHOD_NOT_FOUND = -32601,      // 方法未找到
    INVALID_PARAMS = -32602,        // 无效参数
    INTERNAL_ERROR = -32603,        // 内部错误

    // 服务器错误范围 -32000 到 -32099
    SERVER_ERROR_START = -32099,
    SERVER_ERROR_END = -32000,

    // 应用层错误码 (正数)
    APPLICATION_ERROR = 0
};

/**
 * @brief JSON-RPC 2.0 错误对象
 *
 * {
 *   "code": -32600,
 *   "message": "Invalid Request",
 *   "data": {...}  // 可选
 * }
 */
struct Error {
    int code;
    std::string message;
    std::optional<nlohmann::json> data;

    Error() : code(0), message("") {}

    Error(ErrorCode errorCode, const std::string& msg, const std::optional<nlohmann::json>& data = std::nullopt)
        : code(static_cast<int>(errorCode)), message(msg), data(data) {}

    Error(int code, const std::string& msg, const std::optional<nlohmann::json>& data = std::nullopt)
        : code(code), message(msg), data(data) {}
};

/**
 * @brief JSON-RPC 2.0 请求对象
 *
 * {
 *   "jsonrpc": "2.0",
 *   "method": "methodName",
 *   "params": [...],  // 可选
 *   "id": 1           // 可选 (通知时省略)
 * }
 */
struct Request {
    std::string jsonrpc = "2.0";
    std::string method;
    nlohmann::json params;           // 可选: array 或 object
    std::optional<nlohmann::json> id; // 可选: string, number, or null

    [[nodiscard]] bool isNotification() const { return !id.has_value(); }
};

/**
 * @brief JSON-RPC 2.0 响应对象
 *
 * {
 *   "jsonrpc": "2.0",
 *   "result": {...},  // 成功时存在
 *   "error": {...},   // 失败时存在
 *   "id": 1
 * }
 */
struct Response {
    std::string jsonrpc = "2.0";
    std::optional<nlohmann::json> result;
    std::optional<Error> error;
    nlohmann::json id;

    [[nodiscard]] bool isSuccess() const { return result.has_value() && !error.has_value(); }
    [[nodiscard]] bool isError() const { return error.has_value(); }
};

/**
 * @brief JSON-RPC 2.0 批量请求/响应
 */
using BatchRequest = std::vector<Request>;
using BatchResponse = std::vector<Response>;

/**
 * @brief JSON-RPC 2.0 序列化异常
 */
class SerializationException : public std::exception {
public:
    explicit SerializationException(const std::string& message) : message_(message) {}

    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }

    [[nodiscard]] const std::string& getMessage() const { return message_; }

private:
    std::string message_;
};

/**
 * @brief JSON-RPC 2.0 序列化/反序列化类
 *
 * 功能特性:
 * - 请求对象的序列化与反序列化
 * - 响应对象的序列化与反序列化
 * - 错误对象的创建
 * - 批量请求/响应支持
 * - 符合 JSON-RPC 2.0 规范验证
 */
class JsonRpcSerialization {
public:
    JsonRpcSerialization() = default;
    ~JsonRpcSerialization() = default;

    // 禁止拷贝和移动
    JsonRpcSerialization(const JsonRpcSerialization&) = delete;
    JsonRpcSerialization& operator=(const JsonRpcSerialization&) = delete;
    JsonRpcSerialization(JsonRpcSerialization&&) = delete;
    JsonRpcSerialization& operator=(JsonRpcSerialization&&) = delete;

    // ========== 请求序列化 ==========

    /**
     * @brief 序列化请求对象为 JSON 字符串
     * @param request 请求对象
     * @return JSON 字符串
     * @throws SerializationException 序列化失败
     */
    [[nodiscard]] std::string serializeRequest(const Request& request) const;

    /**
     * @brief 序列化批量请求对象为 JSON 字符串
     * @param requests 批量请求对象
     * @return JSON 字符串
     * @throws SerializationException 序列化失败
     */
    [[nodiscard]] std::string serializeBatchRequest(const BatchRequest& requests) const;

    // ========== 请求反序列化 ==========

    /**
     * @brief 从 JSON 字符串反序列化请求对象
     * @param jsonStr JSON 字符串
     * @return 请求对象
     * @throws SerializationException 反序列化失败或验证失败
     */
    [[nodiscard]] Request deserializeRequest(const std::string& jsonStr) const;

    /**
     * @brief 从 JSON 对象反序列化请求对象
     * @param json JSON 对象
     * @return 请求对象
     * @throws SerializationException 反序列化失败或验证失败
     */
    [[nodiscard]] Request deserializeRequest(const nlohmann::json& json) const;

    /**
     * @brief 从 JSON 字符串反序列化批量请求对象
     * @param jsonStr JSON 字符串
     * @return 批量请求对象
     * @throws SerializationException 反序列化失败或验证失败
     */
    [[nodiscard]] BatchRequest deserializeBatchRequest(const std::string& jsonStr) const;

    // ========== 响应序列化 ==========

    /**
     * @brief 序列化响应对象为 JSON 字符串
     * @param response 响应对象
     * @return JSON 字符串
     * @throws SerializationException 序列化失败
     */
    [[nodiscard]] std::string serializeResponse(const Response& response) const;

    /**
     * @brief 序列化批量响应对象为 JSON 字符串
     * @param responses 批量响应对象
     * @return JSON 字符串
     * @throws SerializationException 序列化失败
     */
    [[nodiscard]] std::string serializeBatchResponse(const BatchResponse& responses) const;

    // ========== 响应反序列化 ==========

    /**
     * @brief 从 JSON 字符串反序列化响应对象
     * @param jsonStr JSON 字符串
     * @return 响应对象
     * @throws SerializationException 反序列化失败或验证失败
     */
    [[nodiscard]] Response deserializeResponse(const std::string& jsonStr) const;

    /**
     * @brief 从 JSON 对象反序列化响应对象
     * @param json JSON 对象
     * @return 响应对象
     * @throws SerializationException 反序列化失败或验证失败
     */
    [[nodiscard]] Response deserializeResponse(const nlohmann::json& json) const;

    /**
     * @brief 从 JSON 字符串反序列化批量响应对象
     * @param jsonStr JSON 字符串
     * @return 批量响应对象
     * @throws SerializationException 反序列化失败或验证失败
     */
    [[nodiscard]] BatchResponse deserializeBatchResponse(const std::string& jsonStr) const;

    // ========== 成功响应创建 ==========

    /**
     * @brief 创建成功响应
     * @param result 结果数据
     * @param id 请求 ID
     * @return 响应对象
     */
    [[nodiscard]] static Response createSuccessResponse(const nlohmann::json& result, const nlohmann::json& id);

    /**
     * @brief 创建成功响应 (无结果)
     * @param id 请求 ID
     * @return 响应对象
     */
    [[nodiscard]] static Response createSuccessResponse(const nlohmann::json& id);

    // ========== 错误响应创建 ==========

    /**
     * @brief 创建错误响应
     * @param code 错误码
     * @param message 错误消息
     * @param id 请求 ID
     * @param data 额外错误数据
     * @return 响应对象
     */
    [[nodiscard]] static Response createErrorResponse(int code, const std::string& message,
                                                       const nlohmann::json& id,
                                                       const std::optional<nlohmann::json>& data = std::nullopt);

    /**
     * @brief 创建错误响应 (使用 ErrorCode 枚举)
     * @param errorCode 错误码枚举
     * @param id 请求 ID
     * @param data 额外错误数据
     * @return 响应对象
     */
    [[nodiscard]] static Response createErrorResponse(ErrorCode errorCode,
                                                       const nlohmann::json& id,
                                                       const std::optional<nlohmann::json>& data = std::nullopt);

    /**
     * @brief 创建错误响应 (使用 Error 对象)
     * @param error 错误对象
     * @param id 请求 ID
     * @return 响应对象
     */
    [[nodiscard]] static Response createErrorResponse(const Error& error, const nlohmann::json& id);

    // ========== 标准错误创建 ==========

    [[nodiscard]] static Response createParseErrorResponse(const std::string& detail = "");
    [[nodiscard]] static Response createInvalidRequestResponse(const std::string& detail = "");
    [[nodiscard]] static Response createMethodNotFoundResponse(const nlohmann::json& id);
    [[nodiscard]] static Response createInvalidParamsResponse(const nlohmann::json& id, const std::string& detail = "");
    [[nodiscard]] static Response createInternalErrorResponse(const std::string& detail = "");

    // ========== 验证方法 ==========

    /**
     * @brief 验证请求对象是否符合 JSON-RPC 2.0 规范
     * @param request 请求对象
     * @return 是否有效
     */
    [[nodiscard]] static bool validateRequest(const Request& request);

    /**
     * @brief 验证响应对象是否符合 JSON-RPC 2.0 规范
     * @param response 响应对象
     * @return 是否有效
     */
    [[nodiscard]] static bool validateResponse(const Response& response);

    /**
     * @brief 获取错误码对应的默认消息
     * @param code 错误码
     * @return 错误消息
     */
    [[nodiscard]] static std::string getDefaultErrorMessage(int code);

    /**
     * @brief 检测 JSON 字符串是批量请求还是单个请求
     * @param jsonStr JSON 字符串
     * @return true 表示批量请求，false 表示单个请求
     */
    [[nodiscard]] static bool isBatchRequest(const std::string& jsonStr);

private:
    /**
     * @brief 验证 JSON 对象是否符合请求格式
     */
    [[nodiscard]] static bool validateRequestJson(const nlohmann::json& json);

    /**
     * @brief 验证 JSON 对象是否符合响应格式
     */
    [[nodiscard]] static bool validateResponseJson(const nlohmann::json& json);

    /**
     * @brief 验证 JSON-RPC 版本字段
     */
    [[nodiscard]] static bool validateJsonRpcVersion(const nlohmann::json& json);

    /**
     * @brief 格式化 JSON 输出 (带缩进)
     */
    [[nodiscard]] std::string formatJson(const nlohmann::json& json) const;
};

} // namespace mcpserver::json_rpc