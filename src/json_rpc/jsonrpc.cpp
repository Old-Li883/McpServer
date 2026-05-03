#include "jsonrpc.h"
#include <sstream>
#include <stdexcept>

namespace mcpserver::json_rpc {

JsonRpc::JsonRpc(const JsonRpcConfig& config)
    : config_(config) {
}

JsonRpc::~JsonRpc() = default;

// ========== 方法注册 ==========

bool JsonRpc::registerMethod(const std::string& method, MethodHandler handler) {
    if (method.empty() || !handler) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    MethodEntry entry;
    entry.syncHandler = std::move(handler);
    entry.asyncHandler = nullptr;
    entry.isAsync = false;

    methods_[method] = std::move(entry);
    return true;
}

bool JsonRpc::registerAsyncMethod(const std::string& method, AsyncMethodHandler handler) {
    if (method.empty() || !handler) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    MethodEntry entry;
    entry.syncHandler = nullptr;
    entry.asyncHandler = std::move(handler);
    entry.isAsync = true;

    methods_[method] = std::move(entry);
    return true;
}

bool JsonRpc::unregisterMethod(const std::string& method) {
    std::lock_guard<std::mutex> lock(mutex_);
    return methods_.erase(method) > 0;
}

bool JsonRpc::hasMethod(const std::string& method) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return methods_.find(method) != methods_.end();
}

std::vector<std::string> JsonRpc::getRegisteredMethods() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> methods;
    methods.reserve(methods_.size());

    for (const auto& pair : methods_) {
        methods.push_back(pair.first);
    }

    return methods;
}

void JsonRpc::clearMethods() {
    std::lock_guard<std::mutex> lock(mutex_);
    methods_.clear();
}

// ========== 请求处理 ==========

std::optional<Response> JsonRpc::handleRequest(const Request& request) {
    // 严格模式验证
    if (config_.strictMode && !validateRequest(request)) {
        return serialization_.createErrorResponse(ErrorCode::INVALID_REQUEST, nullptr);
    }

    // 通知处理（无 ID，无响应）
    if (request.isNotification()) {
        if (!config_.allowNotification) {
            return serialization_.createErrorResponse(ErrorCode::INVALID_REQUEST, nullptr,
                                                      "Notifications are not allowed");
        }

        // 尝试处理通知，但不返回响应
        try {
            [[maybe_unused]] auto _ = processRequest(request);
        } catch (...) {
            // 通知的错误不返回给客户端
        }

        return std::nullopt;
    }

    // 正常请求处理
    return processRequest(request);
}

std::optional<BatchResponse> JsonRpc::handleBatchRequest(const BatchRequest& requests) {
    if (!config_.allowBatch) {
        // 返回单个错误响应
        BatchResponse responses;
        responses.push_back(serialization_.createErrorResponse(
            ErrorCode::INVALID_REQUEST, nullptr, "Batch requests are not allowed"));
        return responses;
    }

    if (requests.size() > static_cast<size_t>(config_.maxBatchSize)) {
        // 返回单个错误响应
        BatchResponse responses;
        responses.push_back(serialization_.createErrorResponse(
            ErrorCode::INVALID_REQUEST, nullptr, "Batch request too large"));
        return responses;
    }

    BatchResponse responses;

    for (const auto& request : requests) {
        auto response = handleRequest(request);
        if (response.has_value()) {
            responses.push_back(response.value());
        }
    }

    // 如果所有请求都是通知，返回空
    if (responses.empty()) {
        return std::nullopt;
    }

    return responses;
}

std::optional<std::string> JsonRpc::handleRequest(const std::string& jsonStr) {
    try {
        // 检测是否为批量请求
        if (serialization_.isBatchRequest(jsonStr)) {
            auto batchRequest = serialization_.deserializeBatchRequest(jsonStr);
            auto batchResponse = handleBatchRequest(batchRequest);

            if (batchResponse.has_value()) {
                return serialization_.serializeBatchResponse(batchResponse.value());
            }
            return std::nullopt;
        } else {
            auto request = serialization_.deserializeRequest(jsonStr);
            auto response = handleRequest(request);

            if (response.has_value()) {
                return serialization_.serializeResponse(response.value());
            }
            return std::nullopt;
        }
    } catch (const SerializationException& e) {
        // 解析错误返回标准错误响应
        auto errorResp = serialization_.createParseErrorResponse(e.what());
        return serialization_.serializeResponse(errorResp);
    }
}

// ========== 配置管理 ==========

void JsonRpc::setConfig(const JsonRpcConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

JsonRpcConfig JsonRpc::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

// ========== 错误回调 ==========

void JsonRpc::setExceptionHandler(std::function<void(const std::exception&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    exceptionHandler_ = std::move(handler);
}

void JsonRpc::setMethodNotFoundHandler(std::function<Response(const std::string&, const nlohmann::json&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    methodNotFoundHandler_ = std::move(handler);
}

// ========== 便捷方法 ==========

Request JsonRpc::createNotification(const std::string& method, const nlohmann::json& params) {
    Request request;
    request.method = method;
    request.params = params.is_null() ? nlohmann::json::array() : params;
    request.id = std::nullopt;  // 通知没有 ID
    return request;
}

Request JsonRpc::createRequest(const std::string& method, const nlohmann::json& params, const nlohmann::json& id) {
    Request request;
    request.method = method;
    request.params = params.is_null() ? nlohmann::json::array() : params;
    request.id = id;
    return request;
}

Request JsonRpc::createRequest(const std::string& method, const nlohmann::json& params, const std::string& id) {
    return createRequest(method, params, nlohmann::json(id));
}

// ========== 私有方法 ==========

Response JsonRpc::processRequest(const Request& request) {
    // 检查方法是否存在
    MethodEntry* entry = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = methods_.find(request.method);
        if (it != methods_.end()) {
            entry = &it->second;
        }
    }

    if (entry == nullptr) {
        // 方法未找到
        if (methodNotFoundHandler_) {
            return methodNotFoundHandler_(request.method, request.params);
        }
        return serialization_.createMethodNotFoundResponse(
            request.id.value_or(nlohmann::json()));
    }

    try {
        nlohmann::json requestId = request.id.value_or(nlohmann::json());

        if (entry->isAsync) {
            // 异步方法处理
            // 注意：这里简化处理，实际应用中可能需要更复杂的异步机制
            // 对于同步 API，我们无法直接处理异步，返回错误
            return serialization_.createErrorResponse(
                ErrorCode::INTERNAL_ERROR, requestId,
                "Async method cannot be handled synchronously");
        } else {
            // 同步方法处理
            auto result = entry->syncHandler(request.method, request.params);
            return serialization_.createSuccessResponse(result, requestId);
        }
    } catch (const std::exception& e) {
        // 方法执行异常
        if (exceptionHandler_) {
            exceptionHandler_(e);
        }

        nlohmann::json requestId = request.id.value_or(nlohmann::json());
        return serialization_.createErrorResponse(
            ErrorCode::INTERNAL_ERROR, requestId, e.what());
    } catch (...) {
        // 未知异常
        nlohmann::json requestId = request.id.value_or(nlohmann::json());
        return serialization_.createErrorResponse(
            ErrorCode::INTERNAL_ERROR, requestId, "Unknown error");
    }
}

bool JsonRpc::validateRequest(const Request& request) const {
    return serialization_.validateRequest(request);
}

std::optional<MethodResult> JsonRpc::invokeMethod(const std::string& method, const nlohmann::json& params) {
    MethodEntry* entry = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = methods_.find(method);
        if (it != methods_.end()) {
            entry = &it->second;
        }
    }

    if (entry == nullptr || entry->isAsync) {
        return std::nullopt;
    }

    try {
        return entry->syncHandler(method, params);
    } catch (...) {
        return std::nullopt;
    }
}

Response JsonRpc::createErrorResponse(ErrorCode code, const nlohmann::json& id, const std::string& detail) const {
    if (detail.empty()) {
        return serialization_.createErrorResponse(code, id);
    }

    nlohmann::json data = detail;
    return serialization_.createErrorResponse(code, id, data);
}

} // namespace mcpserver::json_rpc
