/**
 * @file test_jsonrpc.cpp
 * @brief JSON-RPC 2.0 模块单元测试
 */

#include "gtest/gtest.h"
#include "../src/json_rpc/jsonrpc_serialization.h"
#include "../src/json_rpc/jsonrpc.h"
#include "../src/json_rpc/stdio_jsonrpc.h"
#include <thread>
#include <chrono>

using namespace mcpserver::json_rpc;

// ========== JsonRpcSerialization 测试 ==========

TEST(JsonRpcSerializationTest, SerializeRequest) {
    JsonRpcSerialization serialization;
    Request request;
    request.method = "add";
    request.params = nlohmann::json::array({1, 2});
    request.id = 1;

    std::string json = serialization.serializeRequest(request);
    EXPECT_NE(json.find("\"method\":\"add\""), std::string::npos);
    EXPECT_NE(json.find("\"id\":1"), std::string::npos);
}

TEST(JsonRpcSerializationTest, DeserializeRequest) {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"subtract","params":[42,23],"id":1})";
    Request request = serialization.deserializeRequest(jsonStr);

    EXPECT_STREQ(request.method.c_str(), "subtract");
    EXPECT_EQ(request.params[0], 42);
}

TEST(JsonRpcSerializationTest, SerializeResponse) {
    JsonRpcSerialization serialization;
    Response response = serialization.createSuccessResponse(42, 1);
    std::string json = serialization.serializeResponse(response);
    EXPECT_NE(json.find("\"result\":42"), std::string::npos);
}

TEST(JsonRpcSerializationTest, ErrorResponse) {
    JsonRpcSerialization serialization;
    Response error = serialization.createErrorResponse(ErrorCode::METHOD_NOT_FOUND, 1);
    EXPECT_TRUE(error.isError());
    EXPECT_EQ(error.error.value().code, -32601);
}

TEST(JsonRpcSerializationTest, Notification) {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"update","params":[1,2,3]})";
    Request request = serialization.deserializeRequest(jsonStr);
    EXPECT_TRUE(request.isNotification());
}

TEST(JsonRpcSerializationTest, BatchRequest) {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"([{"jsonrpc":"2.0","method":"add","params":[1,2],"id":1}])";
    BatchRequest batch = serialization.deserializeBatchRequest(jsonStr);
    EXPECT_EQ(batch.size(), 1);
}

TEST(JsonRpcSerializationTest, ValidateRequest) {
    JsonRpcSerialization serialization;
    Request validRequest;
    validRequest.method = "test";
    validRequest.id = 1;
    EXPECT_TRUE(serialization.validateRequest(validRequest));
}

TEST(JsonRpcSerializationTest, NamedParams) {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"subtract","params":{"subtrahend":23,"minuend":42},"id":3})";
    Request request = serialization.deserializeRequest(jsonStr);
    EXPECT_TRUE(request.params.is_object());
}

TEST(JsonRpcSerializationTest, StringId) {
    JsonRpcSerialization serialization;
    std::string jsonStr = R"({"jsonrpc":"2.0","method":"getData","id":"req-123"})";
    Request request = serialization.deserializeRequest(jsonStr);
    EXPECT_STREQ(request.id.value().get<std::string>().c_str(), "req-123");
}

// ========== JsonRpc 测试 ==========

TEST(JsonRpcTest, RegisterAndCallMethod) {
    JsonRpc rpc;
    rpc.registerMethod("add", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() + params[1].get<int>();
    });

    Request request = JsonRpc::createRequest("add", nlohmann::json::array({3, 5}), 1);
    auto response = rpc.handleRequest(request);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response.value().result.value(), 8);
}

TEST(JsonRpcTest, MethodNotFound) {
    JsonRpc rpc;
    Request request = JsonRpc::createRequest("unknownMethod", nullptr, 1);
    auto response = rpc.handleRequest(request);
    EXPECT_TRUE(response.value().isError());
    EXPECT_EQ(response.value().error.value().code, -32601);
}

TEST(JsonRpcTest, NotificationNoResponse) {
    JsonRpc rpc;
    rpc.registerMethod("notify", [](const std::string&, const nlohmann::json&) {
        return nlohmann::json("processed");
    });

    Request request = JsonRpc::createNotification("notify", nullptr);
    auto response = rpc.handleRequest(request);
    EXPECT_FALSE(response.has_value());
}

TEST(JsonRpcTest, BatchRequestHandling) {
    JsonRpc rpc;
    rpc.registerMethod("double", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() * 2;
    });

    BatchRequest batch;
    batch.push_back(JsonRpc::createRequest("double", nlohmann::json::array({5}), 1));
    batch.push_back(JsonRpc::createRequest("double", nlohmann::json::array({10}), 2));

    auto responses = rpc.handleBatchRequest(batch);
    ASSERT_TRUE(responses.has_value());
    EXPECT_EQ(responses.value().size(), 2);
}

TEST(JsonRpcTest, ExceptionHandling) {
    JsonRpc rpc;
    bool exceptionCaught = false;
    rpc.setExceptionHandler([&](const std::exception&) { exceptionCaught = true; });

    rpc.registerMethod("throwError", [](const std::string&, const nlohmann::json&) -> nlohmann::json {
        throw std::runtime_error("Test exception");
    });

    Request request = JsonRpc::createRequest("throwError", nullptr, 1);
    auto response = rpc.handleRequest(request);
    EXPECT_TRUE(exceptionCaught);
    EXPECT_TRUE(response.value().isError());
}

TEST(JsonRpcTest, UnregisterMethod) {
    JsonRpc rpc;
    rpc.registerMethod("tempMethod", [](const std::string&, const nlohmann::json&) {
        return nlohmann::json();
    });

    EXPECT_TRUE(rpc.hasMethod("tempMethod"));
    rpc.unregisterMethod("tempMethod");
    EXPECT_FALSE(rpc.hasMethod("tempMethod"));
}

TEST(JsonRpcTest, StringAndJsonParams) {
    JsonRpc rpc;
    rpc.registerMethod("concat", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<std::string>() + " " + params[1].get<std::string>();
    });

    Request request = JsonRpc::createRequest("concat", nlohmann::json::array({"Hello", "World"}), 1);
    auto response = rpc.handleRequest(request);
    EXPECT_STREQ(response.value().result.value().get<std::string>().c_str(), "Hello World");
}

TEST(JsonRpcTest, NestedJsonObject) {
    JsonRpc rpc;
    rpc.registerMethod("processUser", [](const std::string&, const nlohmann::json& params) {
        return nlohmann::json{{"greeting", "Hello, " + params["name"].get<std::string>()},
                               {"agePlusOne", params["age"].get<int>() + 1}};
    });

    nlohmann::json userParams = {{"name", "Alice"}, {"age", 30}};
    Request request = JsonRpc::createRequest("processUser", userParams, 1);
    auto response = rpc.handleRequest(request);
    EXPECT_EQ(response.value().result.value()["greeting"], "Hello, Alice");
}

TEST(JsonRpcTest, StrictMode) {
    JsonRpcConfig config;
    config.strictMode = true;
    JsonRpc rpc(config);

    Request invalidRequest;
    invalidRequest.method = "";
    invalidRequest.id = 1;

    auto response = rpc.handleRequest(invalidRequest);
    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response.value().isError());
}

// ========== 端到端测试 ==========

TEST(EndToEndTest, RequestResponse) {
    JsonRpcSerialization serialization;
    JsonRpc rpc;

    rpc.registerMethod("multiply", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() * params[1].get<int>();
    });

    std::string requestJson = R"({"jsonrpc":"2.0","method":"multiply","params":[6,7],"id":1})";
    auto responseJson = rpc.handleRequest(requestJson);

    ASSERT_TRUE(responseJson.has_value());
    Response response = serialization.deserializeResponse(responseJson.value());
    EXPECT_TRUE(response.isSuccess());
    EXPECT_EQ(response.result.value(), 42);
}

TEST(EndToEndTest, ParseError) {
    JsonRpcSerialization serialization;
    JsonRpc rpc;

    std::string invalidJson = R"({"jsonrpc":"2.0","method":"test",invalid})";
    auto response = rpc.handleRequest(invalidJson);

    ASSERT_TRUE(response.has_value());
    Response parsed = serialization.deserializeResponse(response.value());
    EXPECT_TRUE(parsed.isError());
    EXPECT_EQ(parsed.error.value().code, -32700);
}

// ========== 配置和状态测试 ==========

TEST(ConfigurationTest, Configuration) {
    JsonRpcConfig config;
    config.allowNotification = false;
    config.allowBatch = false;
    config.strictMode = false;
    config.maxBatchSize = 50;

    JsonRpc rpc(config);
    JsonRpcConfig retrieved = rpc.getConfig();
    EXPECT_FALSE(retrieved.allowNotification);
    EXPECT_EQ(retrieved.maxBatchSize, 50);
}

TEST(ConfigurationTest, GetRegisteredMethods) {
    JsonRpc rpc;
    rpc.registerMethod("method1", [](const std::string&, const nlohmann::json&) { return nlohmann::json(); });
    rpc.registerMethod("method2", [](const std::string&, const nlohmann::json&) { return nlohmann::json(); });

    auto methods = rpc.getRegisteredMethods();
    EXPECT_EQ(methods.size(), 2);
}

// ========== 错误码测试 ==========

TEST(ErrorCodeTest, ErrorCodes) {
    EXPECT_EQ(static_cast<int>(ErrorCode::PARSE_ERROR), -32700);
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_REQUEST), -32600);
    EXPECT_EQ(static_cast<int>(ErrorCode::METHOD_NOT_FOUND), -32601);

    JsonRpcSerialization serialization;
    EXPECT_STREQ(serialization.getDefaultErrorMessage(-32700).c_str(), "Parse error");
}
