#include <gtest/gtest.h>
#include "../src/mcp/types.h"
#include "../src/mcp/ToolManager.h"
#include "../src/mcp/ResourceManager.h"
#include "../src/mcp/PromptManager.h"
#include "../src/mcp/McpServer.h"
#include "../src/json_rpc/jsonrpc.h"
#include "../src/logger/Logger.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

using namespace mcpserver::mcp;
using namespace mcpserver::json_rpc;

// 辅助函数：安全地从JSON获取字符串值
inline std::string get_json_string(const nlohmann::json& j, const std::string& key, const std::string& default_value) {
    if (j.contains(key) && !j[key].is_null()) {
        return j[key].get<std::string>();
    }
    return default_value;
}

// 辅助函数：安全地从JSON获取整数值
inline int get_json_int(const nlohmann::json& j, const std::string& key, int default_value) {
    if (j.contains(key) && !j[key].is_null()) {
        return j[key].get<int>();
    }
    return default_value;
}

// ========== ToolManager 测试 ==========

class ToolManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tool_manager_ = std::make_unique<ToolManager>();
    }

    void TearDown() override {
        tool_manager_->clearTools();
    }

    std::unique_ptr<ToolManager> tool_manager_;
};

TEST_F(ToolManagerTest, RegisterAndListTool) {
    Tool tool;
    tool.name = "calculator";
    tool.description = "A simple calculator";
    tool.input_schema.properties = {
        {"expression", {{"type", "string"}, {"description", "The name"}}}
    };

    auto handler = [](const std::string&, const nlohmann::json& args) -> ToolResult {
        std::string expr = get_json_string(args, "expression", "");
        return ToolResult::success({ContentItem::text_content("Result: " + expr)});
    };

    EXPECT_TRUE(tool_manager_->registerTool(tool, handler));
    EXPECT_EQ(tool_manager_->getToolCount(), 1);

    auto tools = tool_manager_->listTools();
    EXPECT_EQ(tools.size(), 1);
    EXPECT_EQ(tools[0].name, "calculator");
}

TEST_F(ToolManagerTest, CallTool) {
    tool_manager_->registerTool(
        "echo", "Echo back the input",
        ToolInputSchema{},
        [](const std::string&, const nlohmann::json& args) -> ToolResult {
            std::string message = get_json_string(args, "message", "default");
            return ToolResult::success({ContentItem::text_content("Echo: " + message)});
        }
    );

    nlohmann::json args = {{"message", "Hello!"}};
    auto result = tool_manager_->callTool("echo", args);

    EXPECT_FALSE(result.is_error);
    EXPECT_EQ(result.content.size(), 1);
    EXPECT_EQ(result.content[0].text.value(), "Echo: Hello!");
}

TEST_F(ToolManagerTest, CallNonExistentTool) {
    auto result = tool_manager_->callTool("nonexistent", nullptr);
    EXPECT_TRUE(result.is_error);
}

TEST_F(ToolManagerTest, OverrideExistingTool) {
    tool_manager_->registerTool(
        "test", "First version",
        ToolInputSchema{},
        [](const std::string&, const nlohmann::json&) -> ToolResult {
            return ToolResult::success({ContentItem::text_content("v1")});
        }
    );

    tool_manager_->registerTool(
        "test", "Second version",
        ToolInputSchema{},
        [](const std::string&, const nlohmann::json&) -> ToolResult {
            return ToolResult::success({ContentItem::text_content("v2")});
        }
    );

    auto result = tool_manager_->callTool("test", nullptr);
    EXPECT_EQ(result.content[0].text.value(), "v2");
}

TEST_F(ToolManagerTest, UnregisterTool) {
    tool_manager_->registerTool(
        "temp", "Temporary tool",
        ToolInputSchema{},
        [](const std::string&, const nlohmann::json&) -> ToolResult {
            return ToolResult::success({ContentItem::text_content("temp")});
        }
    );

    EXPECT_TRUE(tool_manager_->hasTool("temp"));
    EXPECT_TRUE(tool_manager_->unregisterTool("temp"));
    EXPECT_FALSE(tool_manager_->hasTool("temp"));
}

// ========== ResourceManager 测试 ==========

class ResourceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        resource_manager_ = std::make_unique<ResourceManager>();
    }

    void TearDown() override {
        resource_manager_->clearResources();
    }

    std::unique_ptr<ResourceManager> resource_manager_;
};

TEST_F(ResourceManagerTest, RegisterAndListResource) {
    Resource resource;
    resource.uri = "file:///test.txt";
    resource.name = "Test File";
    resource.description = "A test file resource";
    resource.mime_type = "text/plain";

    auto handler = [](const std::string& uri) -> ResourceContent {
        return ResourceContent::text_resource(uri, "File content here");
    };

    EXPECT_TRUE(resource_manager_->registerResource(resource, handler));
    EXPECT_EQ(resource_manager_->getResourceCount(), 1);

    auto resources = resource_manager_->listResources();
    EXPECT_EQ(resources.size(), 1);
    EXPECT_EQ(resources[0].uri, "file:///test.txt");
}

TEST_F(ResourceManagerTest, ReadResource) {
    resource_manager_->registerResource(
        "file:///data.json",
        "Data File",
        "JSON data",
        "application/json",
        [](const std::string& uri) -> ResourceContent {
            return ResourceContent::text_resource(uri, R"({"key": "value"})");
        }
    );

    auto content = resource_manager_->readResource("file:///data.json");
    EXPECT_TRUE(content.has_value());
    EXPECT_EQ(content->uri, "file:///data.json");
    EXPECT_EQ(content->text, R"({"key": "value"})");
}

TEST_F(ResourceManagerTest, ReadNonExistentResource) {
    auto content = resource_manager_->readResource("file:///nonexistent.txt");
    EXPECT_FALSE(content.has_value());
}

TEST_F(ResourceManagerTest, BinaryResource) {
    std::string base64_data = "SGVsbG8gV29ybGQ=";  // "Hello World" in base64

    resource_manager_->registerResource(
        "file:///image.png",
        "Image",
        std::nullopt,
        "image/png",
        [base64_data](const std::string& uri) -> ResourceContent {
            return ResourceContent::blob_resource(uri, base64_data, "image/png");
        }
    );

    auto content = resource_manager_->readResource("file:///image.png");
    EXPECT_TRUE(content.has_value());
    EXPECT_TRUE(content->blob.has_value());
    EXPECT_EQ(content->blob.value(), base64_data);
    EXPECT_EQ(content->mime_type.value(), "image/png");
}

TEST_F(ResourceManagerTest, UnregisterResource) {
    resource_manager_->registerResource(
        "file:///temp.txt",
        "Temp",
        std::nullopt,
        std::nullopt,
        [](const std::string&) -> ResourceContent {
            return ResourceContent::text_resource("file:///temp.txt", "temp");
        }
    );

    EXPECT_TRUE(resource_manager_->hasResource("file:///temp.txt"));
    EXPECT_TRUE(resource_manager_->unregisterResource("file:///temp.txt"));
    EXPECT_FALSE(resource_manager_->hasResource("file:///temp.txt"));
}

// ========== PromptManager 测试 ==========

class PromptManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        prompt_manager_ = std::make_unique<PromptManager>();
    }

    void TearDown() override {
        prompt_manager_->clearPrompts();
    }

    std::unique_ptr<PromptManager> prompt_manager_;
};

TEST_F(PromptManagerTest, RegisterAndListPrompt)
{
    Prompt prompt;
    prompt.name = "summarize";
    prompt.description = "Summarize the given text";

    // 使用更明确的初始化方式
    PromptArgument arg1;
    arg1.name = "text";
    arg1.description = "Text to summarize";
    arg1.required = true;
    prompt.arguments.push_back(arg1);

    auto handler = [](const std::string&, const nlohmann::json& args) -> PromptResult {
        std::string text = get_json_string(args, "text", "");
        return PromptResult::success({
            PromptMessage::user_text("Please summarize: " + text)
        });
    };

    EXPECT_TRUE(prompt_manager_->registerPrompt(prompt, handler));
    EXPECT_EQ(prompt_manager_->getPromptCount(), 1);

    auto prompts = prompt_manager_->listPrompts();
    EXPECT_EQ(prompts.size(), 1);
    EXPECT_EQ(prompts[0].name, "summarize");
}

TEST_F(PromptManagerTest, GetPrompt) {
    // 使用指针来避免vector初始化问题
    auto handler_lambda = [](const std::string&, const nlohmann::json& args) -> PromptResult {
        std::string name = get_json_string(args, "name", "Guest");
        return PromptResult::success({
            PromptMessage::user_text("Say hello to " + name)
        });
    };

    PromptArgument arg;
    arg.name = "name";
    arg.description = "Name to greet";
    arg.required = true;

    std::vector<PromptArgument> args_vec;
    args_vec.push_back(arg);

    prompt_manager_->registerPrompt(
        "greeting",
        "Generate a greeting",
        args_vec,
        handler_lambda
    );

    nlohmann::json args = {{"name", "Alice"}};
    auto result = prompt_manager_->getPrompt("greeting", args);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_error());
    EXPECT_EQ(result->messages.size(), 1);
}

TEST_F(PromptManagerTest, GetPromptMissingRequiredArgument) {
    Prompt prompt;
    prompt.name = "test";

    PromptArgument arg;
    arg.name = "required_param";
    arg.description = "A required parameter";
    arg.required = true;
    prompt.arguments.push_back(arg);

    auto handler_lambda = [](const std::string&, const nlohmann::json&) -> PromptResult {
        return PromptResult::success({PromptMessage::user_text("OK")});
    };

    prompt_manager_->registerPrompt(prompt, handler_lambda);

    // 不传必需参数
    auto result = prompt_manager_->getPrompt("test", nullptr);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->has_error());
}

TEST_F(PromptManagerTest, GetPromptWithOptionalArguments) {
    PromptArgument arg1, arg2;
    arg1.name = "text";
    arg1.description = "Text to translate";
    arg1.required = true;

    arg2.name = "target_lang";
    arg2.description = "Target language";
    arg2.required = false;

    std::vector<PromptArgument> args_vec;
    args_vec.push_back(arg1);
    args_vec.push_back(arg2);

    auto handler_lambda = [](const std::string&, const nlohmann::json& args) -> PromptResult {
        std::string text = get_json_string(args, "text", "");
        std::string lang = get_json_string(args, "target_lang", "English");
        return PromptResult::success({
            PromptMessage::user_text("Translate '" + text + "' to " + lang)
        });
    };

    prompt_manager_->registerPrompt(
        "translate",
        "Translate text",
        args_vec,
        handler_lambda
    );

    // 只传必需参数
    nlohmann::json args = {{"text", "Hello"}};
    auto result = prompt_manager_->getPrompt("translate", args);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_error());
}

TEST_F(PromptManagerTest, UnregisterPrompt) {
    prompt_manager_->registerPrompt(
        "temp_prompt",
        "Temporary prompt",
        std::vector<PromptArgument>{},
        [](const std::string&, const nlohmann::json&) -> PromptResult {
            return PromptResult::success({PromptMessage::user_text("temp")});
        }
    );

    EXPECT_TRUE(prompt_manager_->hasPrompt("temp_prompt"));
    EXPECT_TRUE(prompt_manager_->unregisterPrompt("temp_prompt"));
    EXPECT_FALSE(prompt_manager_->hasPrompt("temp_prompt"));
}

// ========== 端到端测试 ==========

class McpServerE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化Logger
        mcpserver::Logger::getInstance();

        // 创建JsonRpc
        JsonRpcConfig config;
        config.allowBatch = true;
        json_rpc_ = std::make_unique<JsonRpc>(config);

        // 创建McpServer
        McpServerConfig server_config;
        server_config.server_name = "Test MCP Server";
        server_config.server_version = "1.0.0";
        mcp_server_ = std::make_unique<McpServer>(server_config);

        // 注册一些测试用的工具、资源、提示词
        setupTestResources();

        // 将MCP方法注册到JsonRpc
        mcp_server_->registerMethods(*json_rpc_);
    }

    void TearDown() override {
        mcp_server_->clearAll();
    }

    void setupTestResources() {
        // 注册工具
        mcp_server_->registerTool(
            "add",
            "Add two numbers",
            ToolInputSchema{},
            [](const std::string&, const nlohmann::json& args) -> ToolResult {
                int a = get_json_int(args, "a", 0);
                int b = get_json_int(args, "b", 0);
                return ToolResult::success({
                    ContentItem::text_content(std::to_string(a + b))
                });
            }
        );

        mcp_server_->registerTool(
            "get_weather",
            "Get weather for a city",
            ToolInputSchema{},
            [](const std::string&, const nlohmann::json& args) -> ToolResult {
                std::string city = get_json_string(args, "city", "Unknown");
                return ToolResult::success({
                    ContentItem::text_content("Weather in " + city + ": Sunny, 25°C")
                });
            }
        );

        // 注册资源
        mcp_server_->registerResource(
            "config:///app",
            "Application Config",
            "Application configuration file",
            "application/json",
            [](const std::string& uri) -> ResourceContent {
                return ResourceContent::text_resource(uri, R"({
                    "app_name": "TestApp",
                    "version": "1.0.0",
                    "debug": true
                })");
            }
        );

        mcp_server_->registerResource(
            "log:///app.log",
            "Application Log",
            "Application log file",
            "text/plain",
            [](const std::string& uri) -> ResourceContent {
                return ResourceContent::text_resource(uri,
                    "[INFO] Application started\n[INFO] Processing request\n[INFO] Done");
            }
        );

        // 注册提示词
        PromptArgument lang_arg, focus_arg, code_arg;
        lang_arg.name = "language";
        lang_arg.description = "Programming language";
        lang_arg.required = true;

        focus_arg.name = "focus";
        focus_arg.description = "Review focus area";
        focus_arg.required = false;

        code_arg.name = "code";
        code_arg.description = "Code to explain";
        code_arg.required = true;

        std::vector<PromptArgument> code_review_args;
        code_review_args.push_back(lang_arg);
        code_review_args.push_back(focus_arg);

        mcp_server_->registerPrompt(
            "code_review",
            "Generate code review prompt",
            code_review_args,
            [](const std::string&, const nlohmann::json& args) -> PromptResult {
                std::string lang = get_json_string(args, "language", "Unknown");
                std::string focus = get_json_string(args, "focus", "general");
                return PromptResult::success({
                    PromptMessage::user_text(
                        "Please review this " + lang + " code. Focus on: " + focus
                    )
                });
            }
        );

        std::vector<PromptArgument> explain_code_args;
        explain_code_args.push_back(code_arg);

        mcp_server_->registerPrompt(
            "explain_code",
            "Generate code explanation prompt",
            explain_code_args,
            [](const std::string&, const nlohmann::json& args) -> PromptResult {
                std::string code = get_json_string(args, "code", "");
                return PromptResult::success({
                    PromptMessage::user_text("Explain this code:\n" + code)
                });
            }
        );
    }

    std::unique_ptr<JsonRpc> json_rpc_;
    std::unique_ptr<McpServer> mcp_server_;
};

// 端到端测试：初始化流程
TEST_F(McpServerE2ETest, InitializeFlow) {
    Request request = JsonRpc::createRequest("initialize", nullptr);
    Response response = json_rpc_->handleRequest(request).value();

    EXPECT_TRUE(response.isSuccess());
    EXPECT_TRUE(response.result.has_value());

    auto result = response.result.value();
    EXPECT_TRUE(result.contains("protocolVersion"));
    EXPECT_TRUE(result.contains("serverInfo"));
    EXPECT_TRUE(result.contains("capabilities"));

    EXPECT_EQ(result["serverInfo"]["name"], "Test MCP Server");
    EXPECT_EQ(result["serverInfo"]["version"], "1.0.0");
}

// 端到端测试：工具列表和调用
TEST_F(McpServerE2ETest, ToolsListAndCall) {
    // 1. 列出所有工具
    Request list_request = JsonRpc::createRequest("tools/list", nullptr);
    Response list_response = json_rpc_->handleRequest(list_request).value();

    EXPECT_TRUE(list_response.isSuccess());
    auto tools = list_response.result.value()["tools"];
    EXPECT_GE(tools.size(), 2);  // 至少有add和get_weather

    // 2. 调用add工具
    nlohmann::json add_params = {
        {"name", "add"},
        {"arguments", {{"a", 5}, {"b", 3}}}
    };
    Request add_request = JsonRpc::createRequest("tools/call", add_params);
    Response add_response = json_rpc_->handleRequest(add_request).value();

    EXPECT_TRUE(add_response.isSuccess());
    EXPECT_EQ(add_response.result.value()["content"][0]["text"], "8");

    // 3. 调用get_weather工具
    nlohmann::json weather_params = {
        {"name", "get_weather"},
        {"arguments", {{"city", "Tokyo"}}}
    };
    Request weather_request = JsonRpc::createRequest("tools/call", weather_params);
    Response weather_response = json_rpc_->handleRequest(weather_request).value();

    EXPECT_TRUE(weather_response.isSuccess());
    auto weather_text = weather_response.result.value()["content"][0]["text"].get<std::string>();
    EXPECT_TRUE(weather_text.find("Tokyo") != std::string::npos);
}

// 端到端测试：资源列表和读取
TEST_F(McpServerE2ETest, ResourcesListAndRead) {
    // 1. 列出所有资源
    Request list_request = JsonRpc::createRequest("resources/list", nullptr);
    Response list_response = json_rpc_->handleRequest(list_request).value();

    EXPECT_TRUE(list_response.isSuccess());
    auto resources = list_response.result.value()["resources"];
    EXPECT_GE(resources.size(), 2);  // 至少有config和log

    // 2. 读取config资源
    nlohmann::json config_params = {{"uri", "config:///app"}};
    Request config_request = JsonRpc::createRequest("resources/read", config_params);
    Response config_response = json_rpc_->handleRequest(config_request).value();

    EXPECT_TRUE(config_response.isSuccess());
    auto config_text = config_response.result.value()["text"].get<std::string>();
    EXPECT_TRUE(config_text.find("TestApp") != std::string::npos);

    // 3. 读取log资源
    nlohmann::json log_params = {{"uri", "log:///app.log"}};
    Request log_request = JsonRpc::createRequest("resources/read", log_params);
    Response log_response = json_rpc_->handleRequest(log_request).value();

    EXPECT_TRUE(log_response.isSuccess());
    auto log_text = log_response.result.value()["text"].get<std::string>();
    EXPECT_TRUE(log_text.find("[INFO]") != std::string::npos);
}

// 端到端测试：提示词列表和获取
TEST_F(McpServerE2ETest, PromptsListAndGet) {
    // 1. 列出所有提示词
    Request list_request = JsonRpc::createRequest("prompts/list", nullptr);
    Response list_response = json_rpc_->handleRequest(list_request).value();

    EXPECT_TRUE(list_response.isSuccess());
    auto prompts = list_response.result.value()["prompts"];
    EXPECT_GE(prompts.size(), 2);  // 至少有code_review和explain_code

    // 2. 获取code_review提示词
    nlohmann::json review_params = {
        {"name", "code_review"},
        {"arguments", {{"language", "C++"}, {"focus", "memory safety"}}}
    };
    Request review_request = JsonRpc::createRequest("prompts/get", review_params);
    Response review_response = json_rpc_->handleRequest(review_request).value();

    EXPECT_TRUE(review_response.isSuccess());
    auto messages = review_response.result.value()["messages"];
    EXPECT_GE(messages.size(), 1);
    auto msg_text = messages[0]["content"].get<std::string>();
    EXPECT_TRUE(msg_text.find("C++") != std::string::npos);
    EXPECT_TRUE(msg_text.find("memory safety") != std::string::npos);

    // 3. 获取explain_code提示词
    nlohmann::json explain_params = {
        {"name", "explain_code"},
        {"arguments", {{"code", "int x = 42;"}}}
    };
    Request explain_request = JsonRpc::createRequest("prompts/get", explain_params);
    Response explain_response = json_rpc_->handleRequest(explain_request).value();

    EXPECT_TRUE(explain_response.isSuccess());
}

// 端到端测试：完整工作流（模拟真实使用场景）
TEST_F(McpServerE2ETest, CompleteWorkflow) {
    // 场景：用户想要分析代码性能

    // 步骤1: 初始化
    Request init_req = JsonRpc::createRequest("initialize", nullptr);
    Response init_resp = json_rpc_->handleRequest(init_req).value();
    EXPECT_TRUE(init_resp.isSuccess());

    // 步骤2: 获取代码分析提示词
    nlohmann::json prompt_params = {
        {"name", "code_review"},
        {"arguments", {{"language", "C++"}, {"focus", "performance"}}}
    };
    Request prompt_req = JsonRpc::createRequest("prompts/get", prompt_params);
    Response prompt_resp = json_rpc_->handleRequest(prompt_req).value();
    EXPECT_TRUE(prompt_resp.isSuccess());

    // 步骤3: 读取应用配置
    nlohmann::json config_params = {{"uri", "config:///app"}};
    Request config_req = JsonRpc::createRequest("resources/read", config_params);
    Response config_resp = json_rpc_->handleRequest(config_req).value();
    EXPECT_TRUE(config_resp.isSuccess());

    // 步骤4: 调用计算工具
    nlohmann::json calc_params = {
        {"name", "add"},
        {"arguments", {{"a", 100}, {"b", 250}}}
    };
    Request calc_req = JsonRpc::createRequest("tools/call", calc_params);
    Response calc_resp = json_rpc_->handleRequest(calc_req).value();
    EXPECT_TRUE(calc_resp.isSuccess());
    EXPECT_EQ(calc_resp.result.value()["content"][0]["text"], "350");

    // 步骤5: 再次列出可用工具
    Request tools_req = JsonRpc::createRequest("tools/list", nullptr);
    Response tools_resp = json_rpc_->handleRequest(tools_req).value();
    EXPECT_TRUE(tools_resp.isSuccess());
}

// 端到端测试：批量请求
TEST_F(McpServerE2ETest, BatchRequests) {
    BatchRequest batch;

    // 请求1: 初始化
    batch.push_back(JsonRpc::createRequest("initialize", nullptr));

    // 请求2: 列出工具
    batch.push_back(JsonRpc::createRequest("tools/list", nullptr));

    // 请求3: 列出资源
    batch.push_back(JsonRpc::createRequest("resources/list", nullptr));

    // 请求4: 调用工具
    nlohmann::json call_params = {
        {"name", "add"},
        {"arguments", {{"a", 10}, {"b", 20}}}
    };
    batch.push_back(JsonRpc::createRequest("tools/call", call_params));

    auto batch_response = json_rpc_->handleBatchRequest(batch);
    EXPECT_TRUE(batch_response.has_value());
    EXPECT_EQ(batch_response.value().size(), 4);

    // 验证各个响应
    auto& responses = batch_response.value();
    EXPECT_TRUE(responses[0].isSuccess());  // initialize
    EXPECT_TRUE(responses[1].isSuccess());  // tools/list
    EXPECT_TRUE(responses[2].isSuccess());  // resources/list
    EXPECT_TRUE(responses[3].isSuccess());  // tools/call

    // 验证计算结果
    EXPECT_EQ(responses[3].result.value()["content"][0]["text"], "30");
}

// 端到端测试：错误处理
TEST_F(McpServerE2ETest, ErrorHandling) {
    // 1. 调用不存在的工具
    nlohmann::json bad_tool_params = {
        {"name", "nonexistent_tool"},
        {"arguments", {}}
    };
    Request bad_tool_req = JsonRpc::createRequest("tools/call", bad_tool_params);
    Response bad_tool_resp = json_rpc_->handleRequest(bad_tool_req).value();

    EXPECT_TRUE(bad_tool_resp.isSuccess());
    EXPECT_TRUE(bad_tool_resp.result.value().contains("content"));
    EXPECT_TRUE(bad_tool_resp.result.value()["isError"] == true);

    // 2. 读取不存在的资源
    nlohmann::json bad_resource_params = {{"uri", "nonexistent://resource"}};
    Request bad_resource_req = JsonRpc::createRequest("resources/read", bad_resource_params);
    Response bad_resource_resp = json_rpc_->handleRequest(bad_resource_req).value();

    EXPECT_TRUE(bad_resource_resp.isSuccess());
    EXPECT_TRUE(bad_resource_resp.result.value().contains("error"));

    // 3. 获取不存在的提示词
    nlohmann::json bad_prompt_params = {{"name", "nonexistent_prompt"}};
    Request bad_prompt_req = JsonRpc::createRequest("prompts/get", bad_prompt_params);
    Response bad_prompt_resp = json_rpc_->handleRequest(bad_prompt_req).value();

    EXPECT_TRUE(bad_prompt_resp.isSuccess());
    EXPECT_TRUE(bad_prompt_resp.result.value().contains("error"));
}

// 端到端测试：动态注册新工具
TEST_F(McpServerE2ETest, DynamicToolRegistration) {
    // 动态注册一个新工具
    mcp_server_->registerTool(
        "multiply",
        "Multiply two numbers",
        ToolInputSchema{},
        [](const std::string&, const nlohmann::json& args) -> ToolResult {
            int a = get_json_int(args, "a", 0);
            int b = get_json_int(args, "b", 0);
            return ToolResult::success({
                ContentItem::text_content(std::to_string(a * b))
            });
        }
    );

    // 列出工具，验证新工具已注册
    Request list_req = JsonRpc::createRequest("tools/list", nullptr);
    Response list_resp = json_rpc_->handleRequest(list_req).value();

    auto tools = list_resp.result.value()["tools"];
    bool found = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "multiply") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    // 调用新注册的工具
    nlohmann::json call_params = {
        {"name", "multiply"},
        {"arguments", {{"a", 7}, {"b", 6}}}
    };
    Request call_req = JsonRpc::createRequest("tools/call", call_params);
    Response call_resp = json_rpc_->handleRequest(call_req).value();

    EXPECT_TRUE(call_resp.isSuccess());
    EXPECT_EQ(call_resp.result.value()["content"][0]["text"], "42");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
