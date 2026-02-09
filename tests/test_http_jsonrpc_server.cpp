/**
 * @file test_http_jsonrpc_server.cpp
 * @brief HTTP JSON-RPC 服务器单元测试
 */

#include "gtest/gtest.h"
#include "../src/json_rpc/jsonrpc_serialization.h"
#include "../src/json_rpc/jsonrpc.h"
#include "../src/json_rpc/http_jsonrpc_server.h"
#include <thread>
#include <chrono>
#include <condition_variable>
#include <atomic>

// HTTP 客户端宏（用于测试）
#ifdef _WIN32
#define TEST_HTTP_CLIENT 0
#else
#define TEST_HTTP_CLIENT 1  // 设置为 1 启用实际 HTTP 测试（需要 libcurl）
#endif

#if TEST_HTTP_CLIENT
#include <httplib.h>
#endif

using namespace mcpserver::json_rpc;

// ========== 配置测试 ==========

TEST(HttpJsonRpcServerConfigTest, DefaultConfiguration) {
    HttpJsonRpcServerConfig config;

    EXPECT_EQ(config.host, "0.0.0.0");
    EXPECT_EQ(config.port, 8080);
    EXPECT_EQ(config.threadCount, 4);
    EXPECT_EQ(config.maxPayloadSize, 64 * 1024 * 1024);
    EXPECT_EQ(config.maxConnections, 1000);
    EXPECT_FALSE(config.enableCors);
    EXPECT_EQ(config.corsOrigin, "*");
    EXPECT_TRUE(config.enableSse);
    EXPECT_EQ(config.heartbeatInterval, 30);
}

TEST(HttpJsonRpcServerConfigTest, CustomConfiguration) {
    HttpJsonRpcServerConfig config;
    config.host = "127.0.0.1";
    config.port = 9999;
    config.threadCount = 8;
    config.enableCors = true;
    config.corsOrigin = "http://localhost:3000";

    EXPECT_EQ(config.host, "127.0.0.1");
    EXPECT_EQ(config.port, 9999);
    EXPECT_EQ(config.threadCount, 8);
    EXPECT_TRUE(config.enableCors);
    EXPECT_EQ(config.corsOrigin, "http://localhost:3000");
}

// ========== SSE 事件测试 ==========

TEST(SseEventTest, DefaultEvent) {
    SseEvent event;

    EXPECT_TRUE(event.data.empty());
    EXPECT_TRUE(event.event.empty());
    EXPECT_TRUE(event.id.empty());
    EXPECT_EQ(event.retry, 0);
}

TEST(SseEventTest, EventWithData) {
    SseEvent event(R"({"message":"Hello, SSE!"})");

    EXPECT_EQ(event.data, R"({"message":"Hello, SSE!"})");
    EXPECT_TRUE(event.event.empty());
}

TEST(SseEventTest, CompleteEvent) {
    SseEvent event;
    event.data = "Test data";
    event.event = "update";
    event.id = "msg-123";
    event.retry = 5000;

    EXPECT_EQ(event.data, "Test data");
    EXPECT_EQ(event.event, "update");
    EXPECT_EQ(event.id, "msg-123");
    EXPECT_EQ(event.retry, 5000);
}

// ========== 服务器生命周期测试 ==========

TEST(HttpJsonRpcServerTest, CreateServer) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18080;  // 使用不常用的端口

    HttpJsonRpcServer server(rpc, config);

    EXPECT_FALSE(server.isRunning());
    EXPECT_EQ(server.getServerPort(), 18080);
    EXPECT_EQ(server.getServerAddress(), "0.0.0.0");
}

TEST(HttpJsonRpcServerTest, SetAndGetConfig) {
    JsonRpc rpc;
    HttpJsonRpcServer server(rpc);

    HttpJsonRpcServerConfig config;
    config.port = 19090;
    config.threadCount = 2;

    server.setConfig(config);

    auto retrieved = server.getConfig();
    EXPECT_EQ(retrieved.port, 19090);
    EXPECT_EQ(retrieved.threadCount, 2);
}

TEST(HttpJsonRpcServerTest, StartStopServerAsync) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18081;

    HttpJsonRpcServer server(rpc, config);

    // 启动服务器
    ASSERT_TRUE(server.startAsync());

    // 等待服务器完全启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(server.isRunning());

    // 停止服务器
    server.stop();

    // 等待服务器完全停止
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(server.isRunning());
}

TEST(HttpJsonRpcServerTest, CannotStartTwice) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18082;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 第二次启动应该失败
    EXPECT_FALSE(server.startAsync());

    server.stop();
    server.wait();
}

TEST(HttpJsonRpcServerTest, MultipleStartStopCycles) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18083;

    for (int i = 0; i < 3; ++i) {
        HttpJsonRpcServer server(rpc, config);

        ASSERT_TRUE(server.startAsync());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_TRUE(server.isRunning());

        server.stop();
        server.wait();

        EXPECT_FALSE(server.isRunning());

        // 使用不同端口避免地址占用
        config.port++;
    }
}

// ========== SSE 端点测试 ==========

TEST(HttpJsonRpcServerTest, RegisterSseEndpoint) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18084;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    bool eventCallbackCalled = false;
    bool connectionCallbackCalled = false;

    auto eventCallback = [&](const std::string&, const std::string&, const SseEvent&) {
        eventCallbackCalled = true;
    };

    auto connectionCallback = [&](const std::string&, const std::string&, bool connected) {
        connectionCallbackCalled = true;
        EXPECT_TRUE(connected);
    };

    ASSERT_TRUE(server.registerSseEndpoint("/events", eventCallback, connectionCallback));

    auto endpoints = server.getSseEndpoints();
    ASSERT_EQ(endpoints.size(), 1);
    EXPECT_EQ(endpoints[0], "/events");
}

TEST(HttpJsonRpcServerTest, RegisterMultipleSseEndpoints) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18085;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, nullptr));
    ASSERT_TRUE(server.registerSseEndpoint("/notifications", nullptr, nullptr));
    ASSERT_TRUE(server.registerSseEndpoint("/updates", nullptr, nullptr));

    auto endpoints = server.getSseEndpoints();
    EXPECT_EQ(endpoints.size(), 3);
}

TEST(HttpJsonRpcServerTest, UnregisterSseEndpoint) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18086;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, nullptr));
    EXPECT_EQ(server.getSseEndpoints().size(), 1);

    ASSERT_TRUE(server.unregisterSseEndpoint("/events"));
    EXPECT_EQ(server.getSseEndpoints().size(), 0);

    // 再次注销应该失败
    EXPECT_FALSE(server.unregisterSseEndpoint("/events"));
}

TEST(HttpJsonRpcServerTest, CannotRegisterDuplicateEndpoint) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18087;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, nullptr));
    EXPECT_FALSE(server.registerSseEndpoint("/events", nullptr, nullptr));
}

TEST(HttpJsonRpcServerTest, SseDisabledCannotRegisterEndpoint) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18088;
    config.enableSse = false;  // 禁用 SSE

    HttpJsonRpcServer server(rpc, config);

    EXPECT_FALSE(server.registerSseEndpoint("/events", nullptr, nullptr));
    EXPECT_FALSE(server.isRunning());

    auto error = server.getLastError();
    EXPECT_FALSE(error.empty());
}

// ========== 自定义 HTTP 处理器测试 ==========

TEST(HttpJsonRpcServerTest, RegisterGetHandler) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18089;

    HttpJsonRpcServer server(rpc, config);

    bool handlerCalled = false;
    auto handler = [&](const std::string&) -> std::string {
        handlerCalled = true;
        return R"({"status":"ok"})";
    };

    ASSERT_TRUE(server.registerGetHandler("/api/status", handler));
}

TEST(HttpJsonRpcServerTest, RegisterPostHandler) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18090;

    HttpJsonRpcServer server(rpc, config);

    bool handlerCalled = false;
    auto handler = [&](const std::string& body) -> std::string {
        handlerCalled = true;
        return R"({"received":")" + body + R"("})";
    };

    ASSERT_TRUE(server.registerPostHandler("/api/echo", handler));
}

TEST(HttpJsonRpcServerTest, StaticFileDirectory) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18091;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.setStaticFileDir("/public", "./public"));
}

// ========== 统计和状态测试 ==========

TEST(HttpJsonRpcServerTest, RequestCount) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18092;

    HttpJsonRpcServer server(rpc, config);

    EXPECT_EQ(server.getProcessedRequestCount(), 0);

    // 启动服务器
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 停止服务器
    server.stop();
    server.wait();

    // 请求数应该保持为 0（因为没有实际发送请求）
    EXPECT_EQ(server.getProcessedRequestCount(), 0);
}

TEST(HttpJsonRpcServerTest, ServerAddressAndPort) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.host = "127.0.0.1";
    config.port = 19000;

    HttpJsonRpcServer server(rpc, config);

    EXPECT_EQ(server.getServerAddress(), "127.0.0.1");
    EXPECT_EQ(server.getServerPort(), 19000);
}

TEST(HttpJsonRpcServerTest, LastError) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18093;
    config.enableSse = false;

    HttpJsonRpcServer server(rpc, config);

    // 尝试注册 SSE 端点（应该失败因为 SSE 被禁用）
    EXPECT_FALSE(server.registerSseEndpoint("/events", nullptr, nullptr));

    auto error = server.getLastError();
    EXPECT_FALSE(error.empty());
}

// ========== JSON-RPC 集成测试 ==========

TEST(HttpJsonRpcIntegrationTest, MethodRegistration) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18094;

    HttpJsonRpcServer server(rpc, config);

    // 注册 JSON-RPC 方法
    rpc.registerMethod("add", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() + params[1].get<int>();
    });

    rpc.registerMethod("echo", [](const std::string&, const nlohmann::json& params) {
        return params;
    });

    auto methods = rpc.getRegisteredMethods();
    EXPECT_EQ(methods.size(), 2);
}

TEST(HttpJsonRpcIntegrationTest, AsyncMethodRegistration) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18095;

    HttpJsonRpcServer server(rpc, config);

    bool asyncCalled = false;

    rpc.registerAsyncMethod("asyncOp",
        [&](const std::string&, const nlohmann::json&,
            const std::function<void(const MethodResult&)>&,
            const std::function<void(int, const std::string&)>&) {
            asyncCalled = true;
        }
    );

    EXPECT_TRUE(rpc.hasMethod("asyncOp"));
}

TEST(HttpJsonRpcIntegrationTest, ServerWithJsonRpcMethods) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18096;
    config.enableCors = true;

    HttpJsonRpcServer server(rpc, config);

    // 注册多个方法
    rpc.registerMethod("multiply", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() * params[1].get<int>();
    });

    rpc.registerMethod("greet", [](const std::string&, const nlohmann::json& params) {
        return "Hello, " + params["name"].get<std::string>() + "!";
    });

    // 启动服务器
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(server.isRunning());

    // 停止服务器
    server.stop();
    server.wait();
}

// ========== 线程安全测试 ==========

TEST(HttpJsonRpcServerTest, ConcurrentOperations) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18097;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    const int threadCount = 4;
    std::vector<std::thread> threads;

    // 多线程注册 SSE 端点
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&, i]() {
            std::string endpoint = "/stream" + std::to_string(i);
            server.registerSseEndpoint(endpoint, nullptr, nullptr);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endpoints = server.getSseEndpoints();
    EXPECT_EQ(endpoints.size(), threadCount);
}

// ========== 配置运行时修改测试 ==========

TEST(HttpJsonRpcServerTest, CannotChangeConfigWhileRunning) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18098;

    HttpJsonRpcServer server(rpc, config);

    // 启动服务器
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 尝试在运行时更改配置
    HttpJsonRpcServerConfig newConfig;
    newConfig.port = 19000;
    server.setConfig(newConfig);

    // 配置应该保持不变（因为服务器正在运行）
    auto retrieved = server.getConfig();
    EXPECT_EQ(retrieved.port, 18098);  // 原始端口

    server.stop();
    server.wait();
}

// ========== 等待测试 ==========

TEST(HttpJsonRpcServerTest, WaitForServerThread) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18099;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 启动一个线程来停止服务器
    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        server.stop();
    });

    // 等待服务器线程结束
    server.wait();

    EXPECT_FALSE(server.isRunning());

    stopper.join();
}

// ========== SSE 客户端计数测试 ==========

TEST(HttpJsonRpcServerTest, SseClientCount) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18100;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, nullptr));

    // 初始客户端数量为 0
    EXPECT_EQ(server.getSseClientCount("/events"), 0);

    // 不存在的端点应该返回 0
    EXPECT_EQ(server.getSseClientCount("/nonexistent"), 0);
}

// ========== 广播事件测试 ==========

TEST(HttpJsonRpcServerTest, BroadcastSseEvent) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18101;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    size_t callbackCount = 0;

    auto eventCallback = [&](const std::string&, const std::string&, const SseEvent&) {
        callbackCount++;
    };

    ASSERT_TRUE(server.registerSseEndpoint("/events", eventCallback, nullptr));

    SseEvent event;
    event.event = "test";
    event.data = R"({"value":123})";

    // 广播到没有客户端的端点应该返回 0
    size_t sent = server.broadcastSseEvent("/events", event);
    EXPECT_EQ(sent, 0);
    EXPECT_EQ(callbackCount, 0);
}

// ========== 带回调的 SSE 测试 ==========

TEST(HttpJsonRpcServerTest, SseWithCallbacks) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18102;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    std::vector<std::string> connectedClients;
    std::vector<std::string> disconnectedClients;

    auto connectionCallback = [&](const std::string& endpoint, const std::string& clientId, bool connected) {
        if (connected) {
            connectedClients.push_back(clientId);
        } else {
            disconnectedClients.push_back(clientId);
        }
    };

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, connectionCallback));
}

// ========== 端点移除后测试 ==========

TEST(HttpJsonRpcServerTest, OperationsAfterEndpointRemoval) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18103;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, nullptr));

    SseEvent event;
    event.data = "test";

    // 移除端点
    ASSERT_TRUE(server.unregisterSseEndpoint("/events"));

    // 操作应该失败
    EXPECT_EQ(server.getSseClientCount("/events"), 0);

    size_t sent = server.broadcastSseEvent("/events", event);
    EXPECT_EQ(sent, 0);
}

// ========== 多个服务器实例测试 ==========

TEST(HttpJsonRpcServerTest, MultipleServerInstances) {
    JsonRpc rpc1, rpc2;

    HttpJsonRpcServerConfig config1;
    config1.port = 18104;

    HttpJsonRpcServerConfig config2;
    config2.port = 18105;

    HttpJsonRpcServer server1(rpc1, config1);
    HttpJsonRpcServer server2(rpc2, config2);

    EXPECT_TRUE(server1.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(server2.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(server1.isRunning());
    EXPECT_TRUE(server2.isRunning());

    server1.stop();
    server2.stop();

    server1.wait();
    server2.wait();

    EXPECT_FALSE(server1.isRunning());
    EXPECT_FALSE(server2.isRunning());
}

// ========== 错误处理测试 ==========

TEST(HttpJsonRpcServerErrorTest, InvalidPortHandling) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 80;  // 可能需要 root 权限

    HttpJsonRpcServer server(rpc, config);

    // 启动可能会失败
    // 注意：这个测试在某些系统上可能会通过（如果不需要 root 权限）
    bool started = server.startAsync();

    if (!started) {
        auto error = server.getLastError();
        EXPECT_FALSE(error.empty());
    } else {
        server.stop();
        server.wait();
    }
}

TEST(HttpJsonRpcServerErrorTest, SendEventToNonExistentEndpoint) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18106;

    HttpJsonRpcServer server(rpc, config);

    SseEvent event;
    event.data = "test";

    // 发送到不存在的端点应该失败
    bool sent = server.sendSseEvent("/nonexistent", "client-123", event);
    EXPECT_FALSE(sent);
}

TEST(HttpJsonRpcServerErrorTest, SendEventToNonExistentClient) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18107;
    config.enableSse = true;

    HttpJsonRpcServer server(rpc, config);

    ASSERT_TRUE(server.registerSseEndpoint("/events", nullptr, nullptr));

    SseEvent event;
    event.data = "test";

    // 发送到不存在的客户端应该失败
    bool sent = server.sendSseEvent("/events", "nonexistent-client", event);
    EXPECT_FALSE(sent);
}

// ========== 端到端功能调用测试 ==========

#if TEST_HTTP_CLIENT

TEST(HttpJsonRpcE2ETest, CompleteRequestResponseFlow) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18200;
    config.host = "127.0.0.1";

    // 注册 JSON-RPC 方法
    rpc.registerMethod("add", [](const std::string&, const nlohmann::json& params) {
        int a = params[0].get<int>();
        int b = params[1].get<int>();
        nlohmann::json result;
        result["sum"] = a + b;
        return result;
    });

    rpc.registerMethod("multiply", [](const std::string&, const nlohmann::json& params) {
        int a = params["a"].get<int>();
        int b = params["b"].get<int>();
        return a * b;
    });

    rpc.registerMethod("greet", [](const std::string&, const nlohmann::json& params) {
        std::string name = params["name"].get<std::string>();
        return "Hello, " + name + "!";
    });

    // 注册一个会抛出异常的方法
    rpc.registerMethod("divide", [](const std::string&, const nlohmann::json& params) {
        int a = params[0].get<int>();
        int b = params[1].get<int>();
        if (b == 0) {
            throw std::runtime_error("Division by zero");
        }
        return a / b;
    });

    // 启动服务器
    HttpJsonRpcServer server(rpc, config);
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 等待服务器完全启动

    // 创建 HTTP 客户端
    httplib::Client client("127.0.0.1", 18200);

    // 测试1: 简单加法请求（数组参数）
    {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = "add";
        request["params"] = {10, 25};
        request["id"] = 1;

        auto res = client.Post("/rpc", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response.contains("result"));
        EXPECT_EQ(response["result"]["sum"], 35);
        EXPECT_EQ(response["id"], 1);
    }

    // 测试2: 乘法请求（对象参数）
    {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = "multiply";
        request["params"] = {{"a", 7}, {"b", 6}};
        request["id"] = 2;

        auto res = client.Post("/rpc", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response.contains("result"));
        EXPECT_EQ(response["result"], 42);
    }

    // 测试3: 字符串拼接
    {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = "greet";
        request["params"] = {{"name", "World"}};
        request["id"] = 3;

        auto res = client.Post("/rpc", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response.contains("result"));
        EXPECT_EQ(response["result"], "Hello, World!");
    }

    // 测试4: 错误处理 - 方法不存在
    {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = "nonexistent";
        request["params"] = {};
        request["id"] = 4;

        auto res = client.Post("/rpc", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32601);  // Method Not Found
    }

    // 测试5: 错误处理 - 参数错误
    {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = "divide";
        request["params"] = {10, 0};  // 除以0
        request["id"] = 5;

        auto res = client.Post("/rpc", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32603);  // Internal Error
    }

    // 测试6: 通知（无 id，无响应）
    {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = "greet";
        request["params"] = {{"name", "Notification"}};
        // 无 id 字段

        auto res = client.Post("/rpc", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        // 通知返回 204 No Content
        EXPECT_EQ(res->status, 204);
        // 响应体为空，不应该尝试解析
    }

    // 测试7: 验证请求计数增加
    size_t requestCount = server.getProcessedRequestCount();
    EXPECT_GT(requestCount, 0);

    // 停止服务器
    server.stop();
    server.wait();
    EXPECT_FALSE(server.isRunning());
}

TEST(HttpJsonRpcE2ETest, BatchRequestHandling) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18201;
    config.host = "127.0.0.1";

    // 注册方法
    rpc.registerMethod("double", [](const std::string&, const nlohmann::json& params) {
        return params[0].get<int>() * 2;
    });

    rpc.registerMethod("square", [](const std::string&, const nlohmann::json& params) {
        int v = params[0].get<int>();
        return v * v;
    });

    // 启动服务器
    HttpJsonRpcServer server(rpc, config);
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 创建 HTTP 客户端
    httplib::Client client("127.0.0.1", 18201);

    // 发送批量请求
    nlohmann::json batch = nlohmann::json::array();
    batch.push_back({
        {"jsonrpc", "2.0"},
        {"method", "double"},
        {"params", {5}},
        {"id", 1}
    });
    batch.push_back({
        {"jsonrpc", "2.0"},
        {"method", "square"},
        {"params", {4}},
        {"id", 2}
    });
    batch.push_back({
        {"jsonrpc", "2.0"},
        {"method", "double"},
        {"params", {10}},
        {"id", 3}
    });

    auto res = client.Post("/rpc", batch.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.is_array());
    EXPECT_EQ(response.size(), 3);

    // 验证每个响应
    EXPECT_EQ(response[0]["result"], 10);
    EXPECT_EQ(response[0]["id"], 1);
    EXPECT_EQ(response[1]["result"], 16);
    EXPECT_EQ(response[1]["id"], 2);
    EXPECT_EQ(response[2]["result"], 20);
    EXPECT_EQ(response[2]["id"], 3);

    server.stop();
    server.wait();
}

TEST(HttpJsonRpcE2ETest, CustomHttpHandler) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18202;
    config.host = "127.0.0.1";

    HttpJsonRpcServer server(rpc, config);

    // 注册自定义 GET 处理器
    server.registerGetHandler("/api/health", [](const std::string&) -> std::string {
        nlohmann::json response;
        response["status"] = "healthy";
        response["timestamp"] = std::time(nullptr);
        return response.dump();
    });

    // 注册自定义 POST 处理器
    server.registerPostHandler("/api/echo", [](const std::string& body) -> std::string {
        nlohmann::json request = nlohmann::json::parse(body);
        nlohmann::json response;
        response["echo"] = request;
        response["received"] = true;
        return response.dump();
    });

    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    httplib::Client client("127.0.0.1", 18202);

    // 测试 GET 请求
    {
        auto res = client.Get("/api/health");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response.contains("status"));
        EXPECT_EQ(response["status"], "healthy");
    }

    // 测试 POST 请求
    {
        nlohmann::json request;
        request["message"] = "Hello from test";
        request["value"] = 42;

        auto res = client.Post("/api/echo", request.dump(), "application/json");
        ASSERT_NE(res, nullptr);
        EXPECT_EQ(res->status, 200);

        auto response = nlohmann::json::parse(res->body);
        EXPECT_TRUE(response["received"]);
        EXPECT_EQ(response["echo"]["message"], "Hello from test");
        EXPECT_EQ(response["echo"]["value"], 42);
    }

    server.stop();
    server.wait();
}

TEST(HttpJsonRpcE2ETest, CorsSupport) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18203;
    config.host = "127.0.0.1";
    config.enableCors = true;
    config.corsOrigin = "http://localhost:3000";

    rpc.registerMethod("test", [](const std::string&, const nlohmann::json&) {
        return "CORS enabled";
    });

    HttpJsonRpcServer server(rpc, config);
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    httplib::Client client("127.0.0.1", 18203);

    // 发送带有 Origin 头的请求
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "test";
    request["params"] = {};
    request["id"] = 1;

    httplib::Headers headers = {
        {"Origin", "http://localhost:3000"}
    };

    auto res = client.Post("/rpc", headers, request.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    // 检查 CORS 头
    bool hasCorsHeader = res->headers.find("Access-Control-Allow-Origin") != res->headers.end();
    EXPECT_TRUE(hasCorsHeader);

    server.stop();
    server.wait();
}

TEST(HttpJsonRpcE2ETest, ServerErrorResponse) {
    JsonRpc rpc;
    HttpJsonRpcServerConfig config;
    config.port = 18204;
    config.host = "127.0.0.1";

    // 注册一个会返回错误的方法
    rpc.registerMethod("validate", [](const std::string&, const nlohmann::json& params) {
        int age = params["age"].get<int>();
        if (age < 0) {
            nlohmann::json error;
            error["valid"] = false;
            error["message"] = "Age cannot be negative";
            return error;
        }
        nlohmann::json result;
        result["valid"] = true;
        result["age"] = age;
        return result;
    });

    HttpJsonRpcServer server(rpc, config);
    ASSERT_TRUE(server.startAsync());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    httplib::Client client("127.0.0.1", 18204);

    // 测试无效输入
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "validate";
    request["params"] = {{"age", -5}};
    request["id"] = 1;

    auto res = client.Post("/rpc", request.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response.contains("result"));
    EXPECT_FALSE(response["result"]["valid"]);
    EXPECT_EQ(response["result"]["message"], "Age cannot be negative");

    // 测试有效输入
    request["params"] = {{"age", 25}};
    res = client.Post("/rpc", request.dump(), "application/json");
    ASSERT_NE(res, nullptr);

    response = nlohmann::json::parse(res->body);
    EXPECT_TRUE(response["result"]["valid"]);
    EXPECT_EQ(response["result"]["age"], 25);

    server.stop();
    server.wait();
}

#endif  // TEST_HTTP_CLIENT

// ========== 主函数 ==========

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
