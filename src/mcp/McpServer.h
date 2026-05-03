#pragma once

#include "types.h"
#include "ToolManager.h"
#include "ResourceManager.h"
#include "PromptManager.h"
#include "../json_rpc/jsonrpc.h"
#include <memory>
#include <string>
#include <functional>

namespace mcpserver::mcp {

/**
 * @brief SSE事件回调
 *
 * 用于向客户端推送实时事件
 *
 * @param event 事件类型
 * @param data 事件数据
 */
using SseEventCallback = std::function<void(const std::string& event, const nlohmann::json& data)>;

/**
 * @brief MCP服务器配置
 */
struct McpServerConfig {
    std::string server_name = "mcp-server";      // 服务器名称
    std::string server_version = "1.0.0";        // 服务器版本
    bool enable_tools = true;                    // 启用工具功能
    bool enable_resources = true;                // 启用资源功能
    bool enable_prompts = true;                  // 启用提示词功能
};

/**
 * @brief MCP服务器核心类
 *
 * 负责管理MCP协议的三大核心功能：Tools、Resources、Prompts
 * 并与JSON-RPC层集成，提供统一的接口
 *
 * 功能特性：
 * - 管理ToolManager、ResourceManager、PromptManager
 * - 处理initialize请求
 * - 注册MCP协议方法到JsonRpc
 * - 支持SSE事件推送（可选）
 * - 线程安全
 */
class McpServer {
public:
    /**
     * @brief 构造函数
     *
     * @param config 服务器配置
     */
    explicit McpServer(const McpServerConfig& config = McpServerConfig());

    /**
     * @brief 析构函数
     */
    ~McpServer();

    // 禁止拷贝和移动
    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;
    McpServer(McpServer&&) = delete;
    McpServer& operator=(McpServer&&) = delete;

    // ========== JSON-RPC集成 ==========

    /**
     * @brief 将MCP方法注册到JsonRpc处理器
     *
     * @param jsonRpc JSON-RPC处理器
     */
    void registerMethods(json_rpc::JsonRpc& jsonRpc);

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

    // ========== SSE事件系统 ==========

    /**
     * @brief 设置SSE事件回调
     *
     * 当需要向客户端推送事件时调用此回调
     *
     * @param callback SSE事件回调函数
     */
    void setSseEventCallback(SseEventCallback callback);

    /**
     * @brief 发送SSE事件
     *
     * @param event 事件类型
     * @param data 事件数据
     */
    void sendSseEvent(const std::string& event, const nlohmann::json& data);

    // ========== 初始化结果 ==========

    /**
     * @brief 获取初始化结果
     *
     * @return 初始化结果JSON
     */
    [[nodiscard]] nlohmann::json getInitializeResult() const;

    /**
     * @brief 获取服务器配置
     */
    [[nodiscard]] McpServerConfig getConfig() const;

    /**
     * @brief 获取服务器能力
     */
    [[nodiscard]] ServerCapabilities getCapabilities() const;

    // ========== 清理方法 ==========

    /**
     * @brief 清除所有已注册的工具、资源、提示词
     */
    void clearAll();

    /**
     * @brief 清除所有工具
     */
    void clearTools();

    /**
     * @brief 清除所有资源
     */
    void clearResources();

    /**
     * @brief 清除所有提示词
     */
    void clearPrompts();

private:
    /**
     * @brief 处理initialize请求
     */
    [[nodiscard]] nlohmann::json handleInitialize(const nlohmann::json& params);

    /**
     * @brief 处理tools/list请求
     */
    [[nodiscard]] nlohmann::json handleToolsList(const nlohmann::json& params);

    /**
     * @brief 处理tools/call请求
     */
    [[nodiscard]] nlohmann::json handleToolsCall(const nlohmann::json& params);

    /**
     * @brief 处理resources/list请求
     */
    [[nodiscard]] nlohmann::json handleResourcesList(const nlohmann::json& params);

    /**
     * @brief 处理resources/read请求
     */
    [[nodiscard]] nlohmann::json handleResourcesRead(const nlohmann::json& params);

    /**
     * @brief 处理prompts/list请求
     */
    [[nodiscard]] nlohmann::json handlePromptsList(const nlohmann::json& params);

    /**
     * @brief 处理prompts/get请求
     */
    [[nodiscard]] nlohmann::json handlePromptsGet(const nlohmann::json& params);

private:
    McpServerConfig config_;
    std::unique_ptr<ToolManager> tool_manager_;
    std::unique_ptr<ResourceManager> resource_manager_;
    std::unique_ptr<PromptManager> prompt_manager_;

    SseEventCallback sse_callback_;
};

} // namespace mcpserver::mcp
