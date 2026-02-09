/**
 * @file test_jsonrpc.cpp
 * @brief JSON-RPC 2.0 模块单元测试
 */

#include "../src/json_rpc/jsonrpc_serialization.h"
#include "../src/json_rpc/jsonrpc.h"
#include "../src/json_rpc/stdio_jsonrpc.h"
#include <iostream>
#include <cassert>
#include <sstream>
#include <thread>
#include <chrono>

using namespace mcpserver::json_rpc;

// ========== 测试辅助工具 ==========

int totalTests = 0;
int passedTests = 0;
int failedTests = 0;

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "  ASSERT_TRUE failed: " #condition << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(std::string(a) == std::string(b))

// ========== JsonRpcSerialization 测试 ==========

bool test_SerializeRequest() {
    JsonRpcSerialization serialization;
    Request request;
    request.method = "add";
    request.params = nlohmann::json::array({1, 2});
    request.id = 1;

    std::string json = serialization.serializeRequest(request);
    ASSERT_TRUE(json.find("\"method\":\"add\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"id\":1") != std::string::npos);
    return true;
}

bool test_DeserializeRequest() {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"subtract","params":[42,23],"id":1})";
    Request request = serialization.deserializeRequest(jsonStr);

    ASSERT_STREQ(request.method.c_str(), "subtract");
    ASSERT_EQ(request.params[0], 42);
    return true;
}

bool test_SerializeResponse() {
    JsonRpcSerialization serialization;
    Response response = serialization.createSuccessResponse(42, 1);
    std::string json = serialization.serializeResponse(response);
    ASSERT_TRUE(json.find("\"result\":42") != std::string::npos);
    return true;
}

bool test_ErrorResponse() {
    JsonRpcSerialization serialization;
    Response error = serialization.createErrorResponse(ErrorCode::METHOD_NOT_FOUND, 1);
    ASSERT_TRUE(error.isError());
    ASSERT_EQ(error.error.value().code, -32601);
    return true;
}

bool test_Notification() {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"update","params":[1,2,3]})";
    Request request = serialization.deserializeRequest(jsonStr);
    ASSERT_TRUE(request.isNotification());
    return true;
}

bool test_BatchRequest() {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"([{"jsonrpc":"2.0","method":"add","params":[1,2],"id":1}])";
    BatchRequest batch = serialization.deserializeBatchRequest(jsonStr);
    ASSERT_EQ(batch.size(), 1);
    return true;
}

bool test_ValidateRequest() {
    JsonRpcSerialization serialization;
    Request validRequest;
    validRequest.method = "test";
    validRequest.id = 1;
    ASSERT_TRUE(serialization.validateRequest(validRequest));
    return true;
}

bool test_NamedParams() {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"subtract","params":{"subtrahend":23,"minuend":42},"id":3})";
    Request request = serialization.deserializeRequest(jsonStr);
    ASSERT_TRUE(request.params.is_object());
    return true;
}

bool test_StringId() {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"getData","id":"req-123"})";
    Request request = serialization.deserializeRequest(jsonStr);
    ASSERT_STREQ(request.id.value().get<std::string>().c_str(), "req-123");
    return true;
}

// ========== JsonRpc 测试 ==========

bool test_RegisterAndCallMethod() {
    JsonRpc rpc;
    rpc.registerMethod("add", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() + params[1].get<int>();
    });

    Request request = JsonRpc::createRequest("add", nlohmann::json::array({3, 5}), 1);
    auto response = rpc.handleRequest(request);
    ASSERT_TRUE(response.has_value());
    ASSERT_EQ(response.value().result.value(), 8);
    return true;
}

bool test_MethodNotFound() {
    JsonRpc rpc;
    Request request = JsonRpc::createRequest("unknownMethod", nullptr, 1);
    auto response = rpc.handleRequest(request);
    ASSERT_TRUE(response.value().isError());
    ASSERT_EQ(response.value().error.value().code, -32601);
    return true;
}

bool test_NotificationNoResponse() {
    JsonRpc rpc;
    rpc.registerMethod("notify", [](const std::string&, const nlohmann::json&) {
        return nlohmann::json("processed");
    });

    Request request = JsonRpc::createNotification("notify", nullptr);
    auto response = rpc.handleRequest(request);
    ASSERT_FALSE(response.has_value());
    return true;
}

bool test_BatchRequestHandling() {
    JsonRpc rpc;
    rpc.registerMethod("double", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() * 2;
    });

    BatchRequest batch;
    batch.push_back(JsonRpc::createRequest("double", nlohmann::json::array({5}), 1));
    batch.push_back(JsonRpc::createRequest("double", nlohmann::json::array({10}), 2));

    auto responses = rpc.handleBatchRequest(batch);
    ASSERT_TRUE(responses.has_value());
    ASSERT_EQ(responses.value().size(), 2);
    return true;
}

bool test_ExceptionHandling() {
    JsonRpc rpc;
    bool exceptionCaught = false;
    rpc.setExceptionHandler([&](const std::exception&) { exceptionCaught = true; });

    rpc.registerMethod("throwError", [](const std::string&, const nlohmann::json&) -> nlohmann::json {
        throw std::runtime_error("Test exception");
    });

    Request request = JsonRpc::createRequest("throwError", nullptr, 1);
    auto response = rpc.handleRequest(request);
    ASSERT_TRUE(exceptionCaught);
    ASSERT_TRUE(response.value().isError());
    return true;
}

bool test_UnregisterMethod() {
    JsonRpc rpc;
    rpc.registerMethod("tempMethod", [](const std::string&, const nlohmann::json&) {
        return nlohmann::json();
    });

    ASSERT_TRUE(rpc.hasMethod("tempMethod"));
    rpc.unregisterMethod("tempMethod");
    ASSERT_FALSE(rpc.hasMethod("tempMethod"));
    return true;
}

bool test_StringAndJsonParams() {
    JsonRpc rpc;
    rpc.registerMethod("concat", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<std::string>() + " " + params[1].get<std::string>();
    });

    Request request = JsonRpc::createRequest("concat", nlohmann::json::array({"Hello", "World"}), 1);
    auto response = rpc.handleRequest(request);
    ASSERT_STREQ(response.value().result.value().get<std::string>().c_str(), "Hello World");
    return true;
}

bool test_NestedJsonObject() {
    JsonRpc rpc;
    rpc.registerMethod("processUser", [](const std::string&, const nlohmann::json& params) {
        return nlohmann::json{{"greeting", "Hello, " + params["name"].get<std::string>()},
                               {"agePlusOne", params["age"].get<int>() + 1}};
    });

    nlohmann::json userParams = {{"name", "Alice"}, {"age", 30}};
    Request request = JsonRpc::createRequest("processUser", userParams, 1);
    auto response = rpc.handleRequest(request);
    ASSERT_EQ(response.value().result.value()["greeting"], "Hello, Alice");
    return true;
}

bool test_StrictMode() {
    JsonRpcConfig config;
    config.strictMode = true;
    JsonRpc rpc(config);

    Request invalidRequest;
    invalidRequest.method = "";
    invalidRequest.id = 1;

    auto response = rpc.handleRequest(invalidRequest);
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(response.value().isError());
    return true;
}

// ========== 端到端测试 ==========

bool test_EndToEndRequestResponse() {
    JsonRpcSerialization serialization;
    JsonRpc rpc;

    rpc.registerMethod("multiply", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() * params[1].get<int>();
    });

    std::string requestJson = R"({"jsonrpc":"2.0","method":"multiply","params":[6,7],"id":1})";
    auto responseJson = rpc.handleRequest(requestJson);

    ASSERT_TRUE(responseJson.has_value());
    Response response = serialization.deserializeResponse(responseJson.value());
    ASSERT_TRUE(response.isSuccess());
    ASSERT_EQ(response.result.value(), 42);
    return true;
}

bool test_ParseError() {
    JsonRpcSerialization serialization;
    JsonRpc rpc;

    std::string invalidJson = R"({"jsonrpc":"2.0","method":"test",invalid})";
    auto response = rpc.handleRequest(invalidJson);

    ASSERT_TRUE(response.has_value());
    Response parsed = serialization.deserializeResponse(response.value());
    ASSERT_TRUE(parsed.isError());
    ASSERT_EQ(parsed.error.value().code, -32700);
    return true;
}

// ========== 配置和状态测试 ==========

bool test_Configuration() {
    JsonRpcConfig config;
    config.allowNotification = false;
    config.allowBatch = false;
    config.strictMode = false;
    config.maxBatchSize = 50;

    JsonRpc rpc(config);
    JsonRpcConfig retrieved = rpc.getConfig();
    ASSERT_FALSE(retrieved.allowNotification);
    ASSERT_EQ(retrieved.maxBatchSize, 50);
    return true;
}

bool test_GetRegisteredMethods() {
    JsonRpc rpc;
    rpc.registerMethod("method1", [](const std::string&, const nlohmann::json&) { return nlohmann::json(); });
    rpc.registerMethod("method2", [](const std::string&, const nlohmann::json&) { return nlohmann::json(); });

    auto methods = rpc.getRegisteredMethods();
    ASSERT_EQ(methods.size(), 2);
    return true;
}

// ========== 错误码测试 ==========

bool test_ErrorCodes() {
    ASSERT_EQ(static_cast<int>(ErrorCode::PARSE_ERROR), -32700);
    ASSERT_EQ(static_cast<int>(ErrorCode::INVALID_REQUEST), -32600);
    ASSERT_EQ(static_cast<int>(ErrorCode::METHOD_NOT_FOUND), -32601);

    JsonRpcSerialization serialization;
    ASSERT_STREQ(serialization.getDefaultErrorMessage(-32700).c_str(), "Parse error");
    return true;
}

// ========== 测试运行器 ==========

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           JSON-RPC 2.0 Module Unit Tests                  ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    std::cout << "========== JsonRpcSerialization Tests ==========" << std::endl;
    totalTests++; std::cout << "[TEST] SerializeRequest... "; if (test_SerializeRequest()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] DeserializeRequest... "; if (test_DeserializeRequest()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] SerializeResponse... "; if (test_SerializeResponse()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] ErrorResponse... "; if (test_ErrorResponse()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] Notification... "; if (test_Notification()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] BatchRequest... "; if (test_BatchRequest()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] ValidateRequest... "; if (test_ValidateRequest()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] NamedParams... "; if (test_NamedParams()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] StringId... "; if (test_StringId()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }

    std::cout << std::endl << "========== JsonRpc Tests ==========" << std::endl;
    totalTests++; std::cout << "[TEST] RegisterAndCallMethod... "; if (test_RegisterAndCallMethod()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] MethodNotFound... "; if (test_MethodNotFound()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] NotificationNoResponse... "; if (test_NotificationNoResponse()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] BatchRequestHandling... "; if (test_BatchRequestHandling()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] ExceptionHandling... "; if (test_ExceptionHandling()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] UnregisterMethod... "; if (test_UnregisterMethod()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] StringAndJsonParams... "; if (test_StringAndJsonParams()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] NestedJsonObject... "; if (test_NestedJsonObject()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] StrictMode... "; if (test_StrictMode()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }

    std::cout << std::endl << "========== End-to-End Tests ==========" << std::endl;
    totalTests++; std::cout << "[TEST] EndToEndRequestResponse... "; if (test_EndToEndRequestResponse()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] ParseError... "; if (test_ParseError()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }

    std::cout << std::endl << "========== Configuration & State Tests ==========" << std::endl;
    totalTests++; std::cout << "[TEST] Configuration... "; if (test_Configuration()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }
    totalTests++; std::cout << "[TEST] GetRegisteredMethods... "; if (test_GetRegisteredMethods()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }

    std::cout << std::endl << "========== Error Code Tests ==========" << std::endl;
    totalTests++; std::cout << "[TEST] ErrorCodes... "; if (test_ErrorCodes()) { passedTests++; std::cout << "PASSED" << std::endl; } else { failedTests++; std::cout << "FAILED" << std::endl; }

    std::cout << std::endl;
    std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                      Test Summary                          ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Total:   " << totalTests << "                                               ║" << std::endl;
    std::cout << "║  Passed:  " << passedTests << "                                               ║" << std::endl;
    std::cout << "║  Failed:  " << failedTests << "                                                ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;

    return (failedTests == 0) ? 0 : 1;
}
