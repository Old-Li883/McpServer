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
 * @brief 注册 HR 简历筛选工具
 */
void registerHRTools(McpServerRunner& runner) {
    // hr_import_resumes
    runner.registerTool(
        "hr_import_resumes",
        "批量导入 PDF 简历。支持传入单个文件路径或目录路径，自动解析并存储。",
        ToolInputSchema{
            {"type", "object"},
            {
                {"properties", {
                    {"path", {{"type", "string"}, {"description", "PDF 文件路径或包含 PDF 的目录路径"}}},
                    {"force_reparse", {{"type", "boolean"}, {"description", "是否强制重新解析已存在的简历"}, {"default", false}}}
                }},
                {"required", {"path"}}
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            if (!args.contains("path") || !args["path"].is_string()) {
                return ToolResult::error("参数错误：缺少必填字段 path");
            }
            nlohmann::json payload = {{"tool", name}, {"args", args}};
            return ToolResult::success({ContentItem::text_content(payload.dump())});
        }
    );

    // hr_parse_resume
    runner.registerTool(
        "hr_parse_resume",
        "解析单份 PDF 简历，返回结构化信息（姓名、学历、技能等）。如果简历已导入，直接从数据库返回。",
        ToolInputSchema{
            {"type", "object"},
            {
                {"properties", {
                    {"file_path", {{"type", "string"}, {"description", "PDF 文件路径"}}}
                }},
                {"required", {"file_path"}}
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            if (!args.contains("file_path") || !args["file_path"].is_string()) {
                return ToolResult::error("参数错误：缺少必填字段 file_path");
            }
            nlohmann::json payload = {{"tool", name}, {"args", args}};
            return ToolResult::success({ContentItem::text_content(payload.dump())});
        }
    );

    // hr_search_candidates
    runner.registerTool(
        "hr_search_candidates",
        "按条件搜索候选人。支持自然语言查询和结构化过滤。",
        ToolInputSchema{
            {"type", "object"},
            {
                {"properties", {
                    {"query", {{"type", "string"}, {"description", "搜索查询，如'会Python的计算机专业本科生'"}}},
                    {"filters", {
                        {"type", "object"},
                        {"properties", {
                            {"min_degree", {{"type", "string"}}},
                            {"majors", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                            {"skills", {{"type", "array"}, {"items", {{"type", "string"}}}}}
                        }}
                    }},
                    {"top_k", {{"type", "integer"}, {"description", "返回结果数"}, {"default", 5}}}
                }},
                {"required", {"query"}}
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            if (!args.contains("query") || !args["query"].is_string()) {
                return ToolResult::error("参数错误：缺少必填字段 query");
            }
            nlohmann::json payload = {{"tool", name}, {"args", args}};
            return ToolResult::success({ContentItem::text_content(payload.dump())});
        }
    );

    // hr_match_jd
    runner.registerTool(
        "hr_match_jd",
        "根据岗位 JD 匹配候选人。自动解析 JD 需求，在简历库中搜索评分排序，返回最匹配的候选人。",
        ToolInputSchema{
            {"type", "object"},
            {
                {"properties", {
                    {"jd_text", {{"type", "string"}, {"description", "岗位 JD 全文"}}},
                    {"top_k", {{"type", "integer"}, {"description", "返回候选人数量"}, {"default", 5}}}
                }},
                {"required", {"jd_text"}}
            }
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            if (!args.contains("jd_text") || !args["jd_text"].is_string()) {
                return ToolResult::error("参数错误：缺少必填字段 jd_text");
            }
            nlohmann::json payload = {{"tool", name}, {"args", args}};
            return ToolResult::success({ContentItem::text_content(payload.dump())});
        }
    );

    LOG_INFO("Registered {} HR tools", 4);
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
    // 尝试从多个位置加载配置文件
    if (!config.loadFromFile("config.json")) {
        if (!config.loadFromFile("../config.json")) {
            // 如果配置文件加载失败，使用默认配置（HTTP 模式）
            config.setServerMode("http");
        }
    }

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
    registerHRTools(runner);

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
