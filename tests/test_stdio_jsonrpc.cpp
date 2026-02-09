/**
 * @file test_stdio_jsonrpc.cpp
 * @brief StdioJsonRpc 模块单元测试
 */

#include "gtest/gtest.h"
#include "../src/json_rpc/jsonrpc_serialization.h"
#include "../src/json_rpc/jsonrpc.h"
#include "../src/json_rpc/stdio_jsonrpc.h"
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace mcpserver::json_rpc;

// ========== 测试辅助工具 ==========

/**
 * @brief 创建 LSP 格式的消息
 */
std::string createLspMessage(const std::string& jsonContent) {
    std::ostringstream oss;
    oss << "Content-Length: " << jsonContent.length() << "\r\n\r\n" << jsonContent;
    return oss.str();
}

/**
 * @brief 从 LSP 格式消息中提取 JSON 内容
 */
std::string extractJsonFromLsp(const std::string& lspMessage) {
    // 查找 \r\n\r\n
    size_t pos = lspMessage.find("\r\n\r\n");
    if (pos != std::string::npos) {
        return lspMessage.substr(pos + 4);
    }
    return lspMessage;
}

// ========== 基础功能测试 ==========

TEST(StdioJsonRpcTest, CreateInstance) {
    JsonRpc rpc;
    StdioJsonRpcConfig config;
    StdioJsonRpc stdioRpc(rpc, config);

    EXPECT_FALSE(stdioRpc.isRunning());
    EXPECT_EQ(stdioRpc.getReceivedMessageCount(), 0);
    EXPECT_EQ(stdioRpc.getSentMessageCount(), 0);
}

TEST(StdioJsonRpcTest, Configuration) {
    JsonRpc rpc;
    StdioJsonRpcConfig config;
    config.maxMessageSize = 1024 * 1024;
    config.enableDebugLog = false;
    config.useLspFormat = true;

    StdioJsonRpc stdioRpc(rpc, config);

    auto retrievedConfig = stdioRpc.getConfig();
    EXPECT_EQ(retrievedConfig.maxMessageSize, 1024 * 1024);
    EXPECT_TRUE(retrievedConfig.useLspFormat);
}

TEST(StdioJsonRpcTest, SetCallbacks) {
    JsonRpc rpc;
    StdioJsonRpcConfig config;
    StdioJsonRpc stdioRpc(rpc, config);

    bool requestCalled = false;
    bool responseCalled = false;
    bool errorCalled = false;

    stdioRpc.setRequestCallback([&](const std::string&) { requestCalled = true; });
    stdioRpc.setResponseCallback([&](const std::string&) { responseCalled = true; });
    stdioRpc.setErrorCallback([&](const std::string&) { errorCalled = true; });

    // Callbacks are set successfully
    SUCCEED();
}

// ========== LSP 格式测试 ==========

TEST(LspFormatTest, CreateLspMessageFormat) {
    std::string json = R"({"jsonrpc":"2.0","method":"test","id":1})";
    std::string lspMessage = createLspMessage(json);

    EXPECT_EQ(lspMessage.find("Content-Length:"), 0);
    EXPECT_NE(lspMessage.find("\r\n\r\n"), std::string::npos);
    EXPECT_NE(lspMessage.find(json), std::string::npos);
}

TEST(LspFormatTest, ExtractJsonFromLsp) {
    std::string json = R"({"jsonrpc":"2.0","method":"test","id":1})";
    std::string lspMessage = createLspMessage(json);

    std::string extracted = extractJsonFromLsp(lspMessage);
    EXPECT_EQ(extracted, json);
}

// ========== 进程通信测试 ==========

TEST(ProcessCommunicationTest, Echo) {
    // 创建管道用于父子进程通信
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1) << "Failed to create parent_to_child pipe";
    ASSERT_NE(pipe(child_to_parent), -1) << "Failed to create child_to_parent pipe";

    pid_t pid = fork();
    ASSERT_GE(pid, 0) << "Failed to fork";

    if (pid == 0) {
        // 子进程：运行 StdioJsonRpc 服务器
        close(parent_to_child[1]);  // 关闭写端
        close(child_to_parent[0]);  // 关闭读端

        // 重定向 stdin 和 stdout
        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        // 创建 JSON-RPC 服务器
        JsonRpc rpc;
        rpc.registerMethod("echo", [](const std::string&, const nlohmann::json& params) {
            return params["msg"];
        });

        StdioJsonRpcConfig config;
        config.useLspFormat = true;

        StdioJsonRpc stdioRpc(rpc, config);

        // 处理一个请求
        stdioRpc.readAndProcess();
        exit(0);
    } else {
        // 父进程：发送请求并接收响应
        close(parent_to_child[0]);  // 关闭读端
        close(child_to_parent[1]);  // 关闭写端

        // 发送 LSP 格式的请求
        std::string requestJson = R"({"jsonrpc":"2.0","method":"echo","params":{"msg":"Hello, World!"},"id":1})";
        std::string lspRequest = createLspMessage(requestJson);

        ssize_t written = write(parent_to_child[1], lspRequest.c_str(), lspRequest.size());
        EXPECT_EQ(written, static_cast<ssize_t>(lspRequest.size()));

        // 关闭写端，发送 EOF
        close(parent_to_child[1]);

        // 读取响应
        char buffer[4096];
        std::string response;
        ssize_t n;
        while ((n = read(child_to_parent[0], buffer, sizeof(buffer))) > 0) {
            response.append(buffer, n);
        }

        // 验证响应
        ASSERT_FALSE(response.empty());
        if (!response.empty()) {
            std::string jsonResponse = extractJsonFromLsp(response);
            EXPECT_NE(jsonResponse.find("\"result\":\"Hello, World!\""), std::string::npos);
        }

        // 等待子进程结束
        int status;
        waitpid(pid, &status, 0);

        close(child_to_parent[0]);
    }
}

TEST(ProcessCommunicationTest, Add) {
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1);
    ASSERT_NE(pipe(child_to_parent), -1);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // 子进程
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        JsonRpc rpc;
        rpc.registerMethod("add", [](const std::string&, const nlohmann::json& params) {
            return params[0].get<int>() + params[1].get<int>();
        });

        StdioJsonRpcConfig config;
        config.useLspFormat = true;

        StdioJsonRpc stdioRpc(rpc, config);
        stdioRpc.readAndProcess();
        exit(0);
    } else {
        // 父进程
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        std::string requestJson = R"({"jsonrpc":"2.0","method":"add","params":[15,27],"id":1})";
        std::string lspRequest = createLspMessage(requestJson);

        write(parent_to_child[1], lspRequest.c_str(), lspRequest.size());
        close(parent_to_child[1]);

        char buffer[4096];
        std::string response;
        ssize_t n;
        while ((n = read(child_to_parent[0], buffer, sizeof(buffer))) > 0) {
            response.append(buffer, n);
        }

        std::string jsonResponse = extractJsonFromLsp(response);
        EXPECT_NE(jsonResponse.find("\"result\":42"), std::string::npos);

        int status;
        waitpid(pid, &status, 0);

        close(child_to_parent[0]);
    }
}

TEST(ProcessCommunicationTest, MultipleRequests) {
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1);
    ASSERT_NE(pipe(child_to_parent), -1);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // 子进程
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        JsonRpc rpc;
        rpc.registerMethod("multiply", [](const std::string&, const nlohmann::json& params) {
            return params[0].get<int>() * params[1].get<int>();
        });

        StdioJsonRpcConfig config;
        config.useLspFormat = true;

        StdioJsonRpc stdioRpc(rpc, config);

        // 处理多个请求
        for (int i = 0; i < 3; i++) {
            stdioRpc.readAndProcess();
        }

        exit(0);
    } else {
        // 父进程
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        // 发送多个请求
        for (int i = 0; i < 3; i++) {
            std::string requestJson = R"({"jsonrpc":"2.0","method":"multiply","params":[)" +
                                     std::to_string(i + 1) + "," + std::to_string(i + 2) +
                                     R"(],"id":)" + std::to_string(i) + R"(})";
            std::string lspRequest = createLspMessage(requestJson);

            write(parent_to_child[1], lspRequest.c_str(), lspRequest.size());
        }

        close(parent_to_child[1]);

        // 读取所有响应
        char buffer[4096];
        std::string response;
        ssize_t n;
        while ((n = read(child_to_parent[0], buffer, sizeof(buffer))) > 0) {
            response.append(buffer, n);
        }

        // 验证收到多个响应
        size_t count = 0;
        size_t pos = 0;
        while ((pos = response.find("\"result\"", pos)) != std::string::npos) {
            count++;
            pos += 8;
        }
        EXPECT_EQ(count, 3);

        int status;
        waitpid(pid, &status, 0);

        close(child_to_parent[0]);
    }
}

// ========== 错误处理测试 ==========

TEST(ErrorHandlingTest, InvalidJson) {
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1);
    ASSERT_NE(pipe(child_to_parent), -1);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // 子进程
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        JsonRpc rpc;
        StdioJsonRpcConfig config;
        config.useLspFormat = true;

        StdioJsonRpc stdioRpc(rpc, config);
        stdioRpc.readAndProcess();
        exit(0);
    } else {
        // 父进程 - 发送无效的 JSON
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        std::string invalidJson = R"({"jsonrpc":"2.0","method":"test",invalid})";
        std::string lspRequest = createLspMessage(invalidJson);

        write(parent_to_child[1], lspRequest.c_str(), lspRequest.size());
        close(parent_to_child[1]);

        // 读取响应
        char buffer[4096];
        std::string response;
        ssize_t n;
        while ((n = read(child_to_parent[0], buffer, sizeof(buffer))) > 0) {
            response.append(buffer, n);
        }

        // 验证收到错误响应
        std::string jsonResponse = extractJsonFromLsp(response);
        EXPECT_NE(jsonResponse.find("\"error\""), std::string::npos);
        EXPECT_NE(jsonResponse.find("-32700"), std::string::npos);  // Parse error

        int status;
        waitpid(pid, &status, 0);

        close(child_to_parent[0]);
    }
}

TEST(ErrorHandlingTest, MethodNotFound) {
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1);
    ASSERT_NE(pipe(child_to_parent), -1);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // 子进程
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        JsonRpc rpc;
        // 不注册任何方法
        StdioJsonRpcConfig config;
        config.useLspFormat = true;

        StdioJsonRpc stdioRpc(rpc, config);
        stdioRpc.readAndProcess();
        exit(0);
    } else {
        // 父进程
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        std::string requestJson = R"({"jsonrpc":"2.0","method":"unknownMethod","params":[],"id":1})";
        std::string lspRequest = createLspMessage(requestJson);

        write(parent_to_child[1], lspRequest.c_str(), lspRequest.size());
        close(parent_to_child[1]);

        // 读取响应
        char buffer[4096];
        std::string response;
        ssize_t n;
        while ((n = read(child_to_parent[0], buffer, sizeof(buffer))) > 0) {
            response.append(buffer, n);
        }

        // 验证收到方法未找到错误
        std::string jsonResponse = extractJsonFromLsp(response);
        EXPECT_NE(jsonResponse.find("\"error\""), std::string::npos);
        EXPECT_NE(jsonResponse.find("-32601"), std::string::npos);  // Method not found

        int status;
        waitpid(pid, &status, 0);

        close(child_to_parent[0]);
    }
}

// ========== 通知测试 ==========

TEST(NotificationTest, Notification) {
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1);
    ASSERT_NE(pipe(child_to_parent), -1);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // 子进程
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        JsonRpc rpc;
        bool notified = false;
        rpc.registerMethod("notify", [&](const std::string&, const nlohmann::json&) {
            notified = true;
            return nlohmann::json();
        });

        StdioJsonRpcConfig config;
        config.useLspFormat = true;

        StdioJsonRpc stdioRpc(rpc, config);
        stdioRpc.readAndProcess();
        exit(notified ? 0 : 1);
    } else {
        // 父进程 - 发送通知（无 id）
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        std::string notificationJson = R"({"jsonrpc":"2.0","method":"notify","params":{}})";
        std::string lspRequest = createLspMessage(notificationJson);

        write(parent_to_child[1], lspRequest.c_str(), lspRequest.size());
        close(parent_to_child[1]);

        // 通知没有响应，但有 EOF
        char buffer[4096];
        read(child_to_parent[0], buffer, sizeof(buffer));

        int status;
        waitpid(pid, &status, 0);
        EXPECT_EQ(WEXITSTATUS(status), 0);  // 通知被处理

        close(child_to_parent[0]);
    }
}

// ========== 简单行格式测试 ==========

TEST(SimpleLineFormatTest, SimpleLineFormat) {
    int parent_to_child[2];
    int child_to_parent[2];

    ASSERT_NE(pipe(parent_to_child), -1);
    ASSERT_NE(pipe(child_to_parent), -1);

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // 子进程
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);

        close(parent_to_child[0]);
        close(child_to_parent[1]);

        JsonRpc rpc;
        rpc.registerMethod("test", [](const std::string&, const nlohmann::json&) {
            return nlohmann::json("ok");
        });

        StdioJsonRpcConfig config;
        config.useLspFormat = false;  // 使用简单行格式

        StdioJsonRpc stdioRpc(rpc, config);
        stdioRpc.readAndProcess();
        exit(0);
    } else {
        // 父进程
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        std::string requestJson = R"({"jsonrpc":"2.0","method":"test","params":[],"id":1})";
        write(parent_to_child[1], requestJson.c_str(), requestJson.size());
        close(parent_to_child[1]);

        char buffer[4096];
        std::string response;
        ssize_t n;
        while ((n = read(child_to_parent[0], buffer, sizeof(buffer))) > 0) {
            response.append(buffer, n);
        }

        EXPECT_NE(response.find("\"ok\""), std::string::npos);

        int status;
        waitpid(pid, &status, 0);

        close(child_to_parent[0]);
    }
}
