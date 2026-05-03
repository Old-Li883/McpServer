#include "jsonrpc_serialization.h"
#include <stdexcept>
#include <sstream>
#include <vector>

namespace mcpserver::json_rpc {

// ========== 请求序列化 ==========

std::string JsonRpcSerialization::serializeRequest(const Request& request) const {
    try {
        nlohmann::json j;
        j["jsonrpc"] = request.jsonrpc;
        j["method"] = request.method;

        if (!request.params.is_null()) {
            j["params"] = request.params;
        }

        if (request.id.has_value()) {
            j["id"] = request.id.value();
        }

        return formatJson(j);
    } catch (const nlohmann::json::exception& e) {
        throw SerializationException("Failed to serialize request: " + std::string(e.what()));
    }
}

std::string JsonRpcSerialization::serializeBatchRequest(const BatchRequest& requests) const {
    if (requests.empty()) {
        throw SerializationException("Batch request cannot be empty");
    }

    try {
        nlohmann::json::array_t arr;
        for (const auto& request : requests) {
            nlohmann::json j;
            j["jsonrpc"] = request.jsonrpc;
            j["method"] = request.method;

            if (!request.params.is_null()) {
                j["params"] = request.params;
            }

            if (request.id.has_value()) {
                j["id"] = request.id.value();
            }

            arr.push_back(j);
        }

        return formatJson(arr);
    } catch (const nlohmann::json::exception& e) {
        throw SerializationException("Failed to serialize batch request: " + std::string(e.what()));
    }
}

// ========== 请求反序列化 ==========

Request JsonRpcSerialization::deserializeRequest(const std::string& jsonStr) const {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        return deserializeRequest(j);
    } catch (const nlohmann::json::parse_error& e) {
        throw SerializationException("Failed to parse JSON: " + std::string(e.what()));
    } catch (const SerializationException& e) {
        throw;
    }
}

Request JsonRpcSerialization::deserializeRequest(const nlohmann::json& json) const {
    if (!validateRequestJson(json)) {
        throw SerializationException("Invalid JSON-RPC 2.0 request format");
    }

    Request request;
    request.jsonrpc = json.value("jsonrpc", "2.0");
    request.method = json["method"].get<std::string>();

    if (json.contains("params")) {
        request.params = json["params"];
    }

    if (json.contains("id")) {
        request.id = json["id"];
    }

    return request;
}

BatchRequest JsonRpcSerialization::deserializeBatchRequest(const std::string& jsonStr) const {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);

        if (!j.is_array()) {
            throw SerializationException("Batch request must be an array");
        }

        if (j.empty()) {
            throw SerializationException("Batch request cannot be empty");
        }

        BatchRequest requests;
        for (const auto& item : j) {
            requests.push_back(deserializeRequest(item));
        }

        return requests;
    } catch (const nlohmann::json::parse_error& e) {
        throw SerializationException("Failed to parse JSON: " + std::string(e.what()));
    } catch (const SerializationException& e) {
        throw;
    }
}

// ========== 响应序列化 ==========

std::string JsonRpcSerialization::serializeResponse(const Response& response) const {
    try {
        nlohmann::json j;
        j["jsonrpc"] = response.jsonrpc;
        j["id"] = response.id;

        if (response.result.has_value()) {
            j["result"] = response.result.value();
        }

        if (response.error.has_value()) {
            const Error& err = response.error.value();
            nlohmann::json errorObj;
            errorObj["code"] = err.code;
            errorObj["message"] = err.message;

            if (err.data.has_value()) {
                errorObj["data"] = err.data.value();
            }

            j["error"] = errorObj;
        }

        return formatJson(j);
    } catch (const nlohmann::json::exception& e) {
        throw SerializationException("Failed to serialize response: " + std::string(e.what()));
    }
}

std::string JsonRpcSerialization::serializeBatchResponse(const BatchResponse& responses) const {
    try {
        nlohmann::json::array_t arr;
        for (const auto& response : responses) {
            nlohmann::json j;
            j["jsonrpc"] = response.jsonrpc;
            j["id"] = response.id;

            if (response.result.has_value()) {
                j["result"] = response.result.value();
            }

            if (response.error.has_value()) {
                const Error& err = response.error.value();
                nlohmann::json errorObj;
                errorObj["code"] = err.code;
                errorObj["message"] = err.message;

                if (err.data.has_value()) {
                    errorObj["data"] = err.data.value();
                }

                j["error"] = errorObj;
            }

            arr.push_back(j);
        }

        return formatJson(arr);
    } catch (const nlohmann::json::exception& e) {
        throw SerializationException("Failed to serialize batch response: " + std::string(e.what()));
    }
}

// ========== 响应反序列化 ==========

Response JsonRpcSerialization::deserializeResponse(const std::string& jsonStr) const {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        return deserializeResponse(j);
    } catch (const nlohmann::json::parse_error& e) {
        throw SerializationException("Failed to parse JSON: " + std::string(e.what()));
    } catch (const SerializationException& e) {
        throw;
    }
}

Response JsonRpcSerialization::deserializeResponse(const nlohmann::json& json) const {
    if (!validateResponseJson(json)) {
        throw SerializationException("Invalid JSON-RPC 2.0 response format");
    }

    Response response;
    response.jsonrpc = json.value("jsonrpc", "2.0");
    response.id = json["id"];

    if (json.contains("result")) {
        response.result = json["result"];
    }

    if (json.contains("error")) {
        const auto& errorJson = json["error"];
        Error error;
        error.code = errorJson["code"].get<int>();
        error.message = errorJson["message"].get<std::string>();

        if (errorJson.contains("data")) {
            error.data = errorJson["data"];
        }

        response.error = error;
    }

    return response;
}

BatchResponse JsonRpcSerialization::deserializeBatchResponse(const std::string& jsonStr) const {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);

        if (!j.is_array()) {
            throw SerializationException("Batch response must be an array");
        }

        BatchResponse responses;
        for (const auto& item : j) {
            responses.push_back(deserializeResponse(item));
        }

        return responses;
    } catch (const nlohmann::json::parse_error& e) {
        throw SerializationException("Failed to parse JSON: " + std::string(e.what()));
    } catch (const SerializationException& e) {
        throw;
    }
}

// ========== 成功响应创建 ==========

Response JsonRpcSerialization::createSuccessResponse(const nlohmann::json& result, const nlohmann::json& id) {
    Response response;
    response.id = id;
    response.result = result;
    return response;
}

Response JsonRpcSerialization::createSuccessResponse(const nlohmann::json& id) {
    return createSuccessResponse(nlohmann::json(), id);
}

// ========== 错误响应创建 ==========

Response JsonRpcSerialization::createErrorResponse(int code, const std::string& message,
                                                   const nlohmann::json& id,
                                                   const std::optional<nlohmann::json>& data) {
    Response response;
    response.id = id;
    response.error = Error(code, message, data);
    return response;
}

Response JsonRpcSerialization::createErrorResponse(ErrorCode errorCode,
                                                   const nlohmann::json& id,
                                                   const std::optional<nlohmann::json>& data) {
    int code = static_cast<int>(errorCode);
    return createErrorResponse(code, getDefaultErrorMessage(code), id, data);
}

Response JsonRpcSerialization::createErrorResponse(const Error& error, const nlohmann::json& id) {
    Response response;
    response.id = id;
    response.error = error;
    return response;
}

// ========== 标准错误创建 ==========

Response JsonRpcSerialization::createParseErrorResponse(const std::string& detail) {
    auto data = detail.empty() ? std::nullopt : std::optional<nlohmann::json>(detail);
    return createErrorResponse(ErrorCode::PARSE_ERROR, nullptr, data);
}

Response JsonRpcSerialization::createInvalidRequestResponse(const std::string& detail) {
    auto data = detail.empty() ? std::nullopt : std::optional<nlohmann::json>(detail);
    return createErrorResponse(ErrorCode::INVALID_REQUEST, nullptr, data);
}

Response JsonRpcSerialization::createMethodNotFoundResponse(const nlohmann::json& id) {
    return createErrorResponse(ErrorCode::METHOD_NOT_FOUND, id);
}

Response JsonRpcSerialization::createInvalidParamsResponse(const nlohmann::json& id, const std::string& detail) {
    auto data = detail.empty() ? std::nullopt : std::optional<nlohmann::json>(detail);
    return createErrorResponse(ErrorCode::INVALID_PARAMS, id, data);
}

Response JsonRpcSerialization::createInternalErrorResponse(const std::string& detail) {
    auto data = detail.empty() ? std::nullopt : std::optional<nlohmann::json>(detail);
    return createErrorResponse(ErrorCode::INTERNAL_ERROR, nullptr, data);
}

// ========== 验证方法 ==========

bool JsonRpcSerialization::validateRequest(const Request& request) {
    if (request.jsonrpc != "2.0") {
        return false;
    }

    if (request.method.empty()) {
        return false;
    }

    // params 必须是数组或对象（如果存在）
    if (!request.params.is_null() && !request.params.is_array() && !request.params.is_object()) {
        return false;
    }

    return true;
}

bool JsonRpcSerialization::validateResponse(const Response& response) {
    if (response.jsonrpc != "2.0") {
        return false;
    }

    // result 和 error 不能同时存在或同时不存在
    if (response.result.has_value() == response.error.has_value()) {
        return false;
    }

    return true;
}

std::string JsonRpcSerialization::getDefaultErrorMessage(int code) {
    switch (code) {
        case static_cast<int>(ErrorCode::PARSE_ERROR):
            return "Parse error";
        case static_cast<int>(ErrorCode::INVALID_REQUEST):
            return "Invalid Request";
        case static_cast<int>(ErrorCode::METHOD_NOT_FOUND):
            return "Method not found";
        case static_cast<int>(ErrorCode::INVALID_PARAMS):
            return "Invalid params";
        case static_cast<int>(ErrorCode::INTERNAL_ERROR):
            return "Internal error";
        default:
            return "Server error";
    }
}

bool JsonRpcSerialization::isBatchRequest(const std::string& jsonStr) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        return j.is_array();
    } catch (...) {
        return false;
    }
}

// ========== 私有方法 ==========

bool JsonRpcSerialization::validateRequestJson(const nlohmann::json& json) {
    if (!json.is_object()) {
        return false;
    }

    if (!validateJsonRpcVersion(json)) {
        return false;
    }

    if (!json.contains("method") || !json["method"].is_string()) {
        return false;
    }

    if (json.contains("params")) {
        const auto& params = json["params"];
        // params 可以是 array、object 或 null（表示无参数）
        if (!params.is_array() && !params.is_object() && !params.is_null()) {
            return false;
        }
    }

    if (json.contains("id")) {
        const auto& id = json["id"];
        if (!id.is_string() && !id.is_number() && !id.is_null()) {
            return false;
        }
    }

    return true;
}

bool JsonRpcSerialization::validateResponseJson(const nlohmann::json& json) {
    if (!json.is_object()) {
        return false;
    }

    if (!validateJsonRpcVersion(json)) {
        return false;
    }

    if (!json.contains("id")) {
        return false;
    }

    bool hasResult = json.contains("result");
    bool hasError = json.contains("error");

    // result 和 error 必须恰好存在一个
    if (hasResult == hasError) {
        return false;
    }

    if (hasError) {
        const auto& error = json["error"];
        if (!error.is_object()) {
            return false;
        }

        if (!error.contains("code") || !error["code"].is_number()) {
            return false;
        }

        if (!error.contains("message") || !error["message"].is_string()) {
            return false;
        }

        if (error.contains("data")) {
            // data 可以是任何 JSON 值
        }
    }

    return true;
}

bool JsonRpcSerialization::validateJsonRpcVersion(const nlohmann::json& json) {
    if (!json.contains("jsonrpc")) {
        return false;
    }

    const auto& version = json["jsonrpc"];
    if (!version.is_string()) {
        return false;
    }

    return version.get<std::string>() == "2.0";
}

std::string JsonRpcSerialization::formatJson(const nlohmann::json& json) const {
    return json.dump(-1);  // 紧凑格式，无缩进
}

} // namespace mcpserver::json_rpc
