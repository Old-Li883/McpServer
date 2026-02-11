#include "McpServer.h"
#include "../logger/Logger.h"
#include "../logger/LogMacros.h"

namespace mcpserver::mcp {

McpServer::McpServer(const McpServerConfig& config)
    : config_(config) {
    tool_manager_ = std::make_unique<ToolManager>();
    resource_manager_ = std::make_unique<ResourceManager>();
    prompt_manager_ = std::make_unique<PromptManager>();

    LOG_INFO("McpServer initialized: {} v{}", config_.server_name, config_.server_version);
}

McpServer::~McpServer() {
    LOG_INFO("McpServer destroyed");
}

void McpServer::registerMethods(json_rpc::JsonRpc& jsonRpc) {
    // 注册initialize方法
    jsonRpc.registerMethod("initialize", [this](const std::string&, const nlohmann::json& params) {
        return handleInitialize(params);
    });

    // 注册tools相关方法
    if (config_.enable_tools) {
        jsonRpc.registerMethod("tools/list", [this](const std::string&, const nlohmann::json& params) {
            return handleToolsList(params);
        });

        jsonRpc.registerMethod("tools/call", [this](const std::string&, const nlohmann::json& params) {
            return handleToolsCall(params);
        });
    }

    // 注册resources相关方法
    if (config_.enable_resources) {
        jsonRpc.registerMethod("resources/list", [this](const std::string&, const nlohmann::json& params) {
            return handleResourcesList(params);
        });

        jsonRpc.registerMethod("resources/read", [this](const std::string&, const nlohmann::json& params) {
            return handleResourcesRead(params);
        });
    }

    // 注册prompts相关方法
    if (config_.enable_prompts) {
        jsonRpc.registerMethod("prompts/list", [this](const std::string&, const nlohmann::json& params) {
            return handlePromptsList(params);
        });

        jsonRpc.registerMethod("prompts/get", [this](const std::string&, const nlohmann::json& params) {
            return handlePromptsGet(params);
        });
    }

    LOG_INFO("MCP methods registered to JsonRpc");
}

// ========== Tool管理代理方法 ==========

bool McpServer::registerTool(Tool tool, ToolHandler handler) {
    return tool_manager_->registerTool(std::move(tool), std::move(handler));
}

bool McpServer::registerTool(std::string name, std::string description,
                              ToolInputSchema input_schema, ToolHandler handler) {
    return tool_manager_->registerTool(std::move(name), std::move(description),
                                       std::move(input_schema), std::move(handler));
}

bool McpServer::unregisterTool(const std::string& name) {
    return tool_manager_->unregisterTool(name);
}

bool McpServer::hasTool(const std::string& name) const {
    return tool_manager_->hasTool(name);
}

std::vector<Tool> McpServer::listTools() const {
    return tool_manager_->listTools();
}

// ========== Resource管理代理方法 ==========

bool McpServer::registerResource(Resource resource, ResourceReadHandler handler) {
    return resource_manager_->registerResource(std::move(resource), std::move(handler));
}

bool McpServer::registerResource(std::string uri, std::string name,
                                  std::optional<std::string> description,
                                  std::optional<std::string> mime_type,
                                  ResourceReadHandler handler) {
    return resource_manager_->registerResource(std::move(uri), std::move(name),
                                               std::move(description), std::move(mime_type),
                                               std::move(handler));
}

bool McpServer::unregisterResource(const std::string& uri) {
    return resource_manager_->unregisterResource(uri);
}

bool McpServer::hasResource(const std::string& uri) const {
    return resource_manager_->hasResource(uri);
}

std::vector<Resource> McpServer::listResources() const {
    return resource_manager_->listResources();
}

// ========== Prompt管理代理方法 ==========

bool McpServer::registerPrompt(Prompt prompt, PromptGetHandler handler) {
    return prompt_manager_->registerPrompt(std::move(prompt), std::move(handler));
}

bool McpServer::registerPrompt(std::string name,
                                std::optional<std::string> description,
                                std::vector<PromptArgument> arguments,
                                PromptGetHandler handler) {
    return prompt_manager_->registerPrompt(std::move(name), std::move(description),
                                           std::move(arguments), std::move(handler));
}

bool McpServer::unregisterPrompt(const std::string& name) {
    return prompt_manager_->unregisterPrompt(name);
}

bool McpServer::hasPrompt(const std::string& name) const {
    return prompt_manager_->hasPrompt(name);
}

std::vector<Prompt> McpServer::listPrompts() const {
    return prompt_manager_->listPrompts();
}

// ========== SSE事件系统 ==========

void McpServer::setSseEventCallback(SseEventCallback callback) {
    sse_callback_ = std::move(callback);
    LOG_INFO("SSE event callback registered");
}

void McpServer::sendSseEvent(const std::string& event, const nlohmann::json& data) {
    if (sse_callback_) {
        try {
            sse_callback_(event, data);
            LOG_DEBUG("SSE event sent: {}", event);
        } catch (const std::exception& e) {
            LOG_ERROR("Error sending SSE event: {}", e.what());
        }
    }
}

// ========== 初始化结果 ==========

nlohmann::json McpServer::getInitializeResult() const {
    InitializeResult result;
    result.capabilities = getCapabilities();
    result.server_info = config_.server_name;
    result.version = config_.server_version;
    return result.to_json();
}

McpServerConfig McpServer::getConfig() const {
    return config_;
}

ServerCapabilities McpServer::getCapabilities() const {
    ServerCapabilities caps;
    caps.tools = config_.enable_tools && tool_manager_->getToolCount() > 0;
    caps.resources = config_.enable_resources && resource_manager_->getResourceCount() > 0;
    caps.prompts = config_.enable_prompts && prompt_manager_->getPromptCount() > 0;
    return caps;
}

// ========== 清理方法 ==========

void McpServer::clearAll() {
    clearTools();
    clearResources();
    clearPrompts();
    LOG_INFO("All MCP resources cleared");
}

void McpServer::clearTools() {
    tool_manager_->clearTools();
}

void McpServer::clearResources() {
    resource_manager_->clearResources();
}

void McpServer::clearPrompts() {
    prompt_manager_->clearPrompts();
}

// ========== JSON-RPC方法处理器 ==========

nlohmann::json McpServer::handleInitialize(const nlohmann::json& params) {
    (void)params;  // 未使用
    LOG_DEBUG("Handling initialize request");

    InitializeResult result;
    result.capabilities = getCapabilities();
    result.server_info = config_.server_name;
    result.version = config_.server_version;

    return result.to_json();
}

nlohmann::json McpServer::handleToolsList(const nlohmann::json& params) {
    (void)params;  // 未使用
    LOG_DEBUG("Handling tools/list request");

    auto tools = tool_manager_->listTools();
    nlohmann::json::array_t tools_array;

    for (const auto& tool : tools) {
        tools_array.push_back(tool.to_json());
    }

    nlohmann::json result;
    result["tools"] = tools_array;

    return result;
}

nlohmann::json McpServer::handleToolsCall(const nlohmann::json& params) {
    LOG_DEBUG("Handling tools/call request");

    if (!params.contains("name")) {
        return nlohmann::json({
            {"content", nlohmann::json::array({{
                {"type", "text"},
                {"text", "Missing 'name' parameter in tools/call request"}
            }})},
            {"isError", true}
        });
    }

    std::string name = params["name"].get<std::string>();
    nlohmann::json arguments = params.value("arguments", nlohmann::json::object());

    auto result = tool_manager_->callTool(name, arguments);
    return result.to_json();
}

nlohmann::json McpServer::handleResourcesList(const nlohmann::json& params) {
    (void)params;  // 未使用
    LOG_DEBUG("Handling resources/list request");

    auto resources = resource_manager_->listResources();
    nlohmann::json::array_t resources_array;

    for (const auto& resource : resources) {
        resources_array.push_back(resource.to_json());
    }

    nlohmann::json result;
    result["resources"] = resources_array;

    return result;
}

nlohmann::json McpServer::handleResourcesRead(const nlohmann::json& params) {
    LOG_DEBUG("Handling resources/read request");

    if (!params.contains("uri")) {
        return nlohmann::json({
            {"error", "Missing 'uri' parameter in resources/read request"}
        });
    }

    std::string uri = params["uri"].get<std::string>();
    auto content = resource_manager_->readResource(uri);

    if (!content.has_value()) {
        return nlohmann::json({
            {"error", "Failed to read resource: " + uri}
        });
    }

    return content.value().to_json();
}

nlohmann::json McpServer::handlePromptsList(const nlohmann::json& params) {
    (void)params;  // 未使用
    LOG_DEBUG("Handling prompts/list request");

    try {
        auto prompts = prompt_manager_->listPrompts();
        nlohmann::json::array_t prompts_array;

        for (const auto& prompt : prompts) {
            prompts_array.push_back(prompt.to_json());
        }

        nlohmann::json result;
        result["prompts"] = prompts_array;

        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in handlePromptsList: {}", e.what());
        return nlohmann::json({
            {"error", std::string("Exception: ") + e.what()}
        });
    }
}

nlohmann::json McpServer::handlePromptsGet(const nlohmann::json& params) {
    LOG_DEBUG("Handling prompts/get request");

    try {
        if (!params.contains("name")) {
            return nlohmann::json({
                {"error", "Missing 'name' parameter in prompts/get request"}
            });
        }

        std::string name = params["name"].get<std::string>();
        LOG_DEBUG("Getting prompt: {}", name);

        // Use an empty object as default instead of nullptr
        nlohmann::json arguments;
        if (params.contains("arguments")) {
            arguments = params["arguments"];
        } else {
            arguments = nlohmann::json::object();
        }

        auto result = prompt_manager_->getPrompt(name, arguments);

        if (!result.has_value()) {
            return nlohmann::json({
                {"error", "Failed to get prompt: " + name}
            });
        }

        return result.value().to_json();
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in handlePromptsGet: {}", e.what());
        return nlohmann::json({
            {"error", std::string("Exception: ") + e.what()}
        });
    }
}

} // namespace mcpserver::mcp
