#include "McpServerRunner.h"
#include "../logger/Logger.h"
#include "../logger/LogMacros.h"
#include <thread>
#include <csignal>

namespace mcpserver::mcp {

using json_rpc::JsonRpc;
using json_rpc::HttpJsonRpcServer;
using json_rpc::HttpJsonRpcServerConfig;
using json_rpc::StdioJsonRpc;
using json_rpc::StdioJsonRpcConfig;

// ========== 信号处理（用于优雅退出） ==========

static std::function<void()> g_signalHandler;
static void signalHandler(int signal) {
    if (g_signalHandler) {
        g_signalHandler();
    }
    exit(0);
}

// ========== McpServerRunner 实现 ==========

McpServerRunner::McpServerRunner(const McpServerRunnerConfig& config)
    : config_(config),
      running_(false) {
    initialize();
}

McpServerRunner::~McpServerRunner() {
    stop();
}

void McpServerRunner::initialize() {
    // 创建JSON-RPC处理器
    jsonRpc_ = std::make_unique<JsonRpc>();

    // 创建MCP服务器
    McpServerConfig mcpConfig;
    mcpConfig.server_name = config_.server_name;
    mcpConfig.server_version = config_.server_version;
    mcpConfig.enable_tools = config_.enable_tools;
    mcpConfig.enable_resources = config_.enable_resources;
    mcpConfig.enable_prompts = config_.enable_prompts;

    mcpServer_ = std::make_unique<McpServer>(mcpConfig);

    // 注册MCP方法到JSON-RPC
    mcpServer_->registerMethods(*jsonRpc_);

    // 根据模式创建传输层
    if (config_.mode == ServerMode::HTTP || config_.mode == ServerMode::BOTH) {
        HttpJsonRpcServerConfig httpConfig;
        httpConfig.host = config_.host;
        httpConfig.port = config_.port;
        httpServer_ = std::make_unique<HttpJsonRpcServer>(*jsonRpc_, httpConfig);
        LOG_INFO("HTTP server configured for {}:{}", httpConfig.host, httpConfig.port);
    }

    if (config_.mode == ServerMode::STDIO || config_.mode == ServerMode::BOTH) {
        StdioJsonRpcConfig stdioConfig;
        stdioConfig.useLspFormat = config_.useLspFormat;
        stdioConfig.enableDebugLog = config_.enableDebugLog;
        stdioTransport_ = std::make_unique<StdioJsonRpc>(*jsonRpc_, stdioConfig);
        LOG_INFO("Stdio transport configured (LSP format: {})", stdioConfig.useLspFormat);
    }

    // 设置信号处理（仅在非stdio模式下，stdio模式由父进程管理）
    if (config_.mode != ServerMode::STDIO) {
        g_signalHandler = [this]() {
            LOG_INFO("Received signal, shutting down...");
            stop();
        };
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

    LOG_INFO("McpServerRunner initialized in {} mode", serverModeToString(config_.mode));
}

// ========== Tool管理代理方法 ==========

bool McpServerRunner::registerTool(Tool tool, ToolHandler handler) {
    return mcpServer_->registerTool(std::move(tool), std::move(handler));
}

bool McpServerRunner::registerTool(std::string name, std::string description,
                                   ToolInputSchema input_schema, ToolHandler handler) {
    return mcpServer_->registerTool(std::move(name), std::move(description),
                                   std::move(input_schema), std::move(handler));
}

bool McpServerRunner::unregisterTool(const std::string& name) {
    return mcpServer_->unregisterTool(name);
}

bool McpServerRunner::hasTool(const std::string& name) const {
    return mcpServer_->hasTool(name);
}

std::vector<Tool> McpServerRunner::listTools() const {
    return mcpServer_->listTools();
}

// ========== Resource管理代理方法 ==========

bool McpServerRunner::registerResource(Resource resource, ResourceReadHandler handler) {
    return mcpServer_->registerResource(std::move(resource), std::move(handler));
}

bool McpServerRunner::registerResource(std::string uri, std::string name,
                                      std::optional<std::string> description,
                                      std::optional<std::string> mime_type,
                                      ResourceReadHandler handler) {
    return mcpServer_->registerResource(std::move(uri), std::move(name),
                                       std::move(description), std::move(mime_type),
                                       std::move(handler));
}

bool McpServerRunner::unregisterResource(const std::string& uri) {
    return mcpServer_->unregisterResource(uri);
}

bool McpServerRunner::hasResource(const std::string& uri) const {
    return mcpServer_->hasResource(uri);
}

std::vector<Resource> McpServerRunner::listResources() const {
    return mcpServer_->listResources();
}

// ========== Prompt管理代理方法 ==========

bool McpServerRunner::registerPrompt(Prompt prompt, PromptGetHandler handler) {
    return mcpServer_->registerPrompt(std::move(prompt), std::move(handler));
}

bool McpServerRunner::registerPrompt(std::string name,
                                    std::optional<std::string> description,
                                    std::vector<PromptArgument> arguments,
                                    PromptGetHandler handler) {
    return mcpServer_->registerPrompt(std::move(name), std::move(description),
                                     std::move(arguments), std::move(handler));
}

bool McpServerRunner::unregisterPrompt(const std::string& name) {
    return mcpServer_->unregisterPrompt(name);
}

bool McpServerRunner::hasPrompt(const std::string& name) const {
    return mcpServer_->hasPrompt(name);
}

std::vector<Prompt> McpServerRunner::listPrompts() const {
    return mcpServer_->listPrompts();
}

// ========== 服务器控制 ==========

bool McpServerRunner::run() {
    if (running_.load()) {
        LOG_WARN("Server is already running");
        return false;
    }

    LOG_INFO("Starting MCP server in {} mode...", serverModeToString(config_.mode));

    bool success = false;
    switch (config_.mode) {
        case ServerMode::HTTP:
            success = runHttpMode();
            break;
        case ServerMode::STDIO:
            success = runStdioMode();
            break;
        case ServerMode::BOTH:
            success = runBothMode();
            break;
    }

    if (success) {
        running_.store(true);
        LOG_INFO("MCP server started successfully");
    } else {
        LOG_ERROR("Failed to start MCP server");
    }

    return success;
}

bool McpServerRunner::startAsync() {
    if (running_.load()) {
        LOG_WARN("Server is already running");
        return false;
    }

    // 仅HTTP模式可以异步启动
    if (config_.mode == ServerMode::HTTP) {
        if (httpServer_->startAsync()) {
            running_.store(true);
            LOG_INFO("HTTP server started asynchronously");
            return true;
        }
        return false;
    }

    // BOTH模式也可以异步启动（HTTP在后台，stdio需要单独处理）
    if (config_.mode == ServerMode::BOTH) {
        if (httpServer_->startAsync()) {
            running_.store(true);
            LOG_INFO("HTTP server started asynchronously (stdio needs to be started separately)");
            return true;
        }
        return false;
    }

    // STDIO模式不支持异步启动（因为它需要占用stdin/stdout）
    LOG_ERROR("STDIO mode cannot be started asynchronously");
    return false;
}

void McpServerRunner::stop() {
    if (!running_.load()) {
        return;
    }

    LOG_INFO("Stopping MCP server...");

    // 停止HTTP服务器
    if (httpServer_ && httpServer_->isRunning()) {
        httpServer_->stop();
        LOG_INFO("HTTP server stopped");
    }

    // 停止stdio传输层
    if (stdioTransport_ && stdioTransport_->isRunning()) {
        stdioTransport_->stop();
        LOG_INFO("Stdio transport stopped");
    }

    // 等待HTTP线程结束
    if (httpThread_ && httpThread_->joinable()) {
        httpThread_->join();
        httpThread_.reset();
    }

    running_.store(false);
    LOG_INFO("MCP server stopped");
}

void McpServerRunner::wait() {
    if (httpThread_ && httpThread_->joinable()) {
        httpThread_->join();
        httpThread_.reset();
    }
}

bool McpServerRunner::isRunning() const {
    if (config_.mode == ServerMode::HTTP) {
        return httpServer_ && httpServer_->isRunning();
    } else if (config_.mode == ServerMode::STDIO) {
        return stdioTransport_ && stdioTransport_->isRunning();
    } else if (config_.mode == ServerMode::BOTH) {
        bool httpRunning = httpServer_ && httpServer_->isRunning();
        bool stdioRunning = stdioTransport_ && stdioTransport_->isRunning();
        return httpRunning || stdioRunning;
    }
    return false;
}

// ========== 状态查询 ==========

std::string McpServerRunner::getServerAddress() const {
    return config_.host;
}

int McpServerRunner::getServerPort() const {
    return config_.port;
}

ServerMode McpServerRunner::getMode() const {
    return config_.mode;
}

McpServerRunnerConfig McpServerRunner::getConfig() const {
    return config_;
}

McpServer& McpServerRunner::getMcpServer() {
    return *mcpServer_;
}

json_rpc::HttpJsonRpcServer* McpServerRunner::getHttpServer() {
    return httpServer_.get();
}

json_rpc::StdioJsonRpc* McpServerRunner::getStdioTransport() {
    return stdioTransport_.get();
}

// ========== 私有方法 ==========

bool McpServerRunner::runHttpMode() {
    LOG_INFO("Starting HTTP server on {}:{}", config_.host, config_.port);
    return httpServer_->start();
}

bool McpServerRunner::runStdioMode() {
    LOG_INFO("Starting stdio transport");
    return stdioTransport_->start();
}

bool McpServerRunner::runBothMode() {
    LOG_INFO("Starting both HTTP and stdio modes");

    // 先启动HTTP服务器（异步）
    if (!httpServer_->startAsync()) {
        LOG_ERROR("Failed to start HTTP server");
        return false;
    }

    LOG_INFO("HTTP server started on {}:{}", config_.host, config_.port);
    LOG_INFO("Starting stdio transport...");

    // 然后启动stdio传输层（阻塞在主线程）
    if (!stdioTransport_->start()) {
        LOG_ERROR("Failed to start stdio transport");
        httpServer_->stop();
        return false;
    }

    return true;
}

} // namespace mcpserver::mcp
