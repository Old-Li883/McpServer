#pragma once

#include "McpServer.h"
#include "../json_rpc/jsonrpc.h"
#include "../json_rpc/stdio_jsonrpc.h"
#include "../json_rpc/http_jsonrpc_server.h"
#include <memory>
#include <string>
#include <functional>

namespace mcpserver::mcp {

/**
 * @brief 服务器运行模式
 */
enum class ServerMode {
    HTTP,   // HTTP模式：通过HTTP提供服务
    STDIO,  // stdio模式：通过标准输入输出提供服务
    BOTH    // both模式：同时支持HTTP和stdio
};

/**
 * @brief 将字符串转换为ServerMode
 */
[[nodiscard]] inline ServerMode stringToServerMode(const std::string& mode) {
    if (mode == "http" || mode == "tcp") {
        return ServerMode::HTTP;
    } else if (mode == "stdio") {
        return ServerMode::STDIO;
    } else if (mode == "both") {
        return ServerMode::BOTH;
    }
    // 默认返回HTTP模式
    return ServerMode::HTTP;
}

/**
 * @brief 将ServerMode转换为字符串
 */
[[nodiscard]] inline std::string serverModeToString(ServerMode mode) {
    switch (mode) {
        case ServerMode::HTTP: return "http";
        case ServerMode::STDIO: return "stdio";
        case ServerMode::BOTH: return "both";
    }
    return "http";
}

/**
 * @brief MCP服务器运行器配置
 */
struct McpServerRunnerConfig {
    ServerMode mode = ServerMode::HTTP;    // 运行模式
    std::string host = "0.0.0.0";          // HTTP监听地址
    int port = 8080;                        // HTTP监听端口
    bool useLspFormat = true;               // stdio模式使用LSP格式
    bool enableDebugLog = false;            // 启用调试日志

    // MCP服务器配置
    std::string server_name = "mcp-server";
    std::string server_version = "1.0.0";
    bool enable_tools = true;
    bool enable_resources = true;
    bool enable_prompts = true;
};

/**
 * @brief MCP服务器统一入口
 *
 * 负责管理MCP服务器的生命周期，支持三种运行模式：
 * - HTTP模式：通过HTTP JSON-RPC提供服务
 * - stdio模式：通过标准输入输出提供MCP服务
 * - both模式：同时运行HTTP和stdio服务
 *
 * 使用示例:
 * @code
 * McpServerRunner runner(config);
 *
 * // 注册工具
 * runner.registerTool("echo", "Echo back the input",
 *     ToolInputSchema{{}, {}},
 *     [](const std::string& name, const nlohmann::json& args) {
 *         std::string text = args.value("text", "default");
 *         return ToolResult::success({ContentItem::text_content(text)});
 *     });
 *
 * // 启动服务器（阻塞）
 * runner.run();
 * @endcode
 */
class McpServerRunner {
public:
    /**
     * @brief 构造函数
     *
     * @param config 运行器配置
     */
    explicit McpServerRunner(const McpServerRunnerConfig& config = McpServerRunnerConfig());

    /**
     * @brief 析构函数
     */
    ~McpServerRunner();

    // 禁止拷贝和移动
    McpServerRunner(const McpServerRunner&) = delete;
    McpServerRunner& operator=(const McpServerRunner&) = delete;
    McpServerRunner(McpServerRunner&&) = delete;
    McpServerRunner& operator=(McpServerRunner&&) = delete;

    // ========== Tool管理代理方法 ==========

    /**
     * @brief 注册工具
     */
    bool registerTool(Tool tool, ToolHandler handler);

    /**
     * @brief 注册工具（便捷方法）
     */
    bool registerTool(std::string name, std::string description,
                      ToolInputSchema input_schema, ToolHandler handler);

    /**
     * @brief 注销工具
     */
    bool unregisterTool(const std::string& name);

    /**
     * @brief 检查工具是否存在
     */
    [[nodiscard]] bool hasTool(const std::string& name) const;

    /**
     * @brief 获取工具列表
     */
    [[nodiscard]] std::vector<Tool> listTools() const;

    // ========== Resource管理代理方法 ==========

    /**
     * @brief 注册资源
     */
    bool registerResource(Resource resource, ResourceReadHandler handler);

    /**
     * @brief 注册资源（便捷方法）
     */
    bool registerResource(std::string uri, std::string name,
                          std::optional<std::string> description,
                          std::optional<std::string> mime_type,
                          ResourceReadHandler handler);

    /**
     * @brief 注销资源
     */
    bool unregisterResource(const std::string& uri);

    /**
     * @brief 检查资源是否存在
     */
    [[nodiscard]] bool hasResource(const std::string& uri) const;

    /**
     * @brief 获取资源列表
     */
    [[nodiscard]] std::vector<Resource> listResources() const;

    // ========== Prompt管理代理方法 ==========

    /**
     * @brief 注册提示词
     */
    bool registerPrompt(Prompt prompt, PromptGetHandler handler);

    /**
     * @brief 注册提示词（便捷方法）
     */
    bool registerPrompt(std::string name,
                        std::optional<std::string> description,
                        std::vector<PromptArgument> arguments,
                        PromptGetHandler handler);

    /**
     * @brief 注销提示词
     */
    bool unregisterPrompt(const std::string& name);

    /**
     * @brief 检查提示词是否存在
     */
    [[nodiscard]] bool hasPrompt(const std::string& name) const;

    /**
     * @brief 获取提示词列表
     */
    [[nodiscard]] std::vector<Prompt> listPrompts() const;

    // ========== 服务器控制 ==========

    /**
     * @brief 启动服务器（阻塞模式）
     *
     * 根据配置的模式启动服务器：
     * - HTTP模式：启动HTTP服务器，阻塞当前线程
     * - stdio模式：启动stdio读取循环，阻塞当前线程
     * - both模式：HTTP在后台线程运行，stdio在主线程运行
     *
     * @return 是否成功启动
     */
    [[nodiscard]] bool run();

    /**
     * @brief 启动服务器（异步模式）
     *
     * 在后台线程中运行服务器，立即返回。
     *
     * @return 是否成功启动
     */
    [[nodiscard]] bool startAsync();

    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 等待服务器线程结束
     *
     * 仅在异步模式下有效。
     */
    void wait();

    /**
     * @brief 检查服务器是否正在运行
     */
    [[nodiscard]] bool isRunning() const;

    // ========== 状态查询 ==========

    /**
     * @brief 获取服务器地址
     */
    [[nodiscard]] std::string getServerAddress() const;

    /**
     * @brief 获取服务器端口
     */
    [[nodiscard]] int getServerPort() const;

    /**
     * @brief 获取运行模式
     */
    [[nodiscard]] ServerMode getMode() const;

    /**
     * @brief 获取配置
     */
    [[nodiscard]] McpServerRunnerConfig getConfig() const;

    /**
     * @brief 获取MCP服务器实例
     *
     * 用于高级操作，如设置SSE回调等
     */
    [[nodiscard]] McpServer& getMcpServer();

    /**
     * @brief 获取HTTP服务器实例（仅在HTTP/BOTH模式下有效）
     */
    [[nodiscard]] json_rpc::HttpJsonRpcServer* getHttpServer();

    /**
     * @brief 获取stdio传输层实例（仅在stdio/BOTH模式下有效）
     */
    [[nodiscard]] json_rpc::StdioJsonRpc* getStdioTransport();

private:
    /**
     * @brief 初始化组件
     */
    void initialize();

    /**
     * @brief 运行HTTP模式
     */
    [[nodiscard]] bool runHttpMode();

    /**
     * @brief 运行stdio模式
     */
    [[nodiscard]] bool runStdioMode();

    /**
     * @brief 运行both模式
     */
    [[nodiscard]] bool runBothMode();

private:
    McpServerRunnerConfig config_;

    // 核心组件
    std::unique_ptr<json_rpc::JsonRpc> jsonRpc_;
    std::unique_ptr<McpServer> mcpServer_;

    // 传输层
    std::unique_ptr<json_rpc::HttpJsonRpcServer> httpServer_;
    std::unique_ptr<json_rpc::StdioJsonRpc> stdioTransport_;

    // 运行状态
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> httpThread_;
};

} // namespace mcpserver::mcp
