#include <iostream>
#include "Config.h"
#include "mcp/McpServerRunner.h"
#include "logger/Logger.h"
#include "logger/LogMacros.h"

using namespace mcpserver::mcp;

/**
 * @brief 创建示例工具
 */
void registerExampleTools(McpServerRunner& runner) {
    // 1. echo工具 - 简单的回显工具
    runner.registerTool(
        "echo",
        "Echo back the input text",
        ToolInputSchema{
            {"type", "object"},
            {
                {"properties", {
                    {"text", {
                        {"type", "string"},
                        {"description", "The text to echo back"}
                    }}
                }},
                {"required", {"text"}}
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            std::string text = args.value("text", "");
            return ToolResult::success({
                ContentItem::text_content("Echo: " + text)
            });
        }
    );

    // 2. add工具 - 计算两个数字的和
    runner.registerTool(
        "add",
        "Add two numbers together",
        ToolInputSchema{
            {"type", "object"},
            {
                {"properties", {
                    {"a", {{"type", "number"}, {"description", "First number"}}},
                    {"b", {{"type", "number"}, {"description", "Second number"}}}
                }},
                {"required", {"a", "b"}}
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            double a = args.value("a", 0.0);
            double b = args.value("b", 0.0);
            double result = a + b;
            return ToolResult::success({
                ContentItem::text_content(std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(result))
            });
        }
    );

    // 3. get_current_time工具 - 获取当前时间
    runner.registerTool(
        "get_current_time",
        "Get the current date and time",
        ToolInputSchema{
            {"type", "object"},
            {{"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}}
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::string timeStr = std::ctime(&time);
            // 移除末尾的换行符
            if (!timeStr.empty() && timeStr.back() == '\n') {
                timeStr.pop_back();
            }
            return ToolResult::success({
                ContentItem::text_content("Current time: " + timeStr)
            });
        }
    );

    LOG_INFO("Registered {} example tools", runner.listTools().size());
}

/**
 * @brief 创建示例资源
 */
void registerExampleResources(McpServerRunner& runner) {
    // 1. 系统信息资源
    runner.registerResource(
        "system://info",
        "System Information",
        "Basic system information",
        "text/plain",
        [](const std::string& uri) -> ResourceContent {
            std::string info = R"({
  "server": "MCP Server",
  "version": "1.0.0",
  "status": "running"
})";
            return ResourceContent::text_resource(uri, info);
        }
    );

    // 2. 帮助资源
    runner.registerResource(
        "help://overview",
        "Help Overview",
        "Available tools and resources",
        "text/plain",
        [](const std::string& uri) -> ResourceContent {
            std::string help = R"(MCP Server Help
============

Available Tools:
- echo: Echo back the input text
- add: Add two numbers together
- get_current_time: Get the current date and time

Available Resources:
- system://info: System information
- help://overview: This help text

Available Prompts:
- summarize: Summarize the given text
- explain: Explain a concept
)";
            return ResourceContent::text_resource(uri, help);
        }
    );

    LOG_INFO("Registered {} example resources", runner.listResources().size());
}

/**
 * @brief 创建示例提示词
 */
void registerExamplePrompts(McpServerRunner& runner) {
    // 1. summarize提示词
    runner.registerPrompt(
        "summarize",
        "Summarize the given text in a concise manner",
        {
            {
                "text",
                "The text to summarize",
                false
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> PromptResult {
            std::string text = args.value("text", "");

            std::string summary = "Please summarize the following text:\n\n" + text;

            return PromptResult::success({
                PromptMessage::user_text(summary)
            });
        }
    );

    // 2. explain提示词
    runner.registerPrompt(
        "explain",
        "Explain a concept in simple terms",
        {
            {
                "concept",
                "The concept to explain",
                false
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> PromptResult {
            std::string concept = args.value("concept", "");

            std::string explanation = "Please explain the concept of '" + concept + "' in simple terms.";

            return PromptResult::success({
                PromptMessage::user_text(explanation)
            });
        }
    );

    LOG_INFO("Registered {} example prompts", runner.listPrompts().size());
}

/**
 * @brief 打印服务器信息
 */
void printServerInfo(const McpServerRunner& runner) {
    std::cout << "========================================\n";
    std::cout << "       MCP Server Started\n";
    std::cout << "========================================\n";
    std::cout << "Mode: ";

    switch (runner.getMode()) {
        case ServerMode::HTTP:
            std::cout << "HTTP\n";
            std::cout << "URL:  http://" << runner.getServerAddress() << ":" << runner.getServerPort() << "\n";
            break;
        case ServerMode::STDIO:
            std::cout << "STDIO (Standard Input/Output)\n";
            std::cout << "Format: LSP/MCP (Content-Length headers)\n";
            break;
        case ServerMode::BOTH:
            std::cout << "BOTH (HTTP + STDIO)\n";
            std::cout << "HTTP URL:  http://" << runner.getServerAddress() << ":" << runner.getServerPort() << "\n";
            std::cout << "STDIO:     Standard Input/Output (LSP format)\n";
            break;
    }

    std::cout << "========================================\n";
    std::cout << "Tools:      " << runner.listTools().size() << " registered\n";
    std::cout << "Resources:  " << runner.listResources().size() << " registered\n";
    std::cout << "Prompts:    " << runner.listPrompts().size() << " registered\n";
    std::cout << "========================================\n";
    std::cout << "Press Ctrl+C to stop the server\n";
    std::cout << "========================================\n";
}

int main(int argc, char* argv[]) {
    // 加载配置
    Config& config = Config::getInstance();

    // 初始化日志系统
    auto& logger = mcpserver::Logger::getInstance();
    logger.initialize(
        config.getLogFilePath().empty() ? "logs/mcpserver.log" : config.getLogFilePath(),
        config.getLogLevel(),
        config.getLogFileSize(),
        config.getLogFileCount(),
        config.getLogConsoleOutput()
    );

    LOG_INFO("MCP Server starting...");
    LOG_INFO("Log level: {}", config.getLogLevel());

    // 创建运行器配置
    McpServerRunnerConfig runnerConfig;
    runnerConfig.mode = stringToServerMode(config.getServerMode());
    runnerConfig.host = "0.0.0.0";
    runnerConfig.port = config.getServerPort();
    runnerConfig.useLspFormat = true;
    runnerConfig.enableDebugLog = false;

    // 创建运行器
    McpServerRunner runner(runnerConfig);

    // 注册示例工具、资源和提示词
    registerExampleTools(runner);
    registerExampleResources(runner);
    registerExamplePrompts(runner);

    // 打印服务器信息
    printServerInfo(runner);

    // 启动服务器（阻塞）
    if (!runner.run()) {
        LOG_ERROR("Failed to start server");
        return 1;
    }

    // 服务器已停止
    LOG_INFO("Server shut down gracefully");

    return 0;
}
