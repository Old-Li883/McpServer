#include "http_jsonrpc_server.h"

#define CPPHTTPLIB_THREAD_POOL_COUNT -1
#include <httplib.h>

#include <sstream>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>
#include <random>

namespace mcpserver::json_rpc {

// ========== Impl 类实现 ==========

class HttpJsonRpcServer::Impl {
public:
    Impl(JsonRpc& rpc, HttpJsonRpcServerConfig config);
    ~Impl();

    // 禁止拷贝和移动
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    // 服务器控制
    [[nodiscard]] bool start();
    [[nodiscard]] bool startAsync();
    void stop();
    void wait();
    [[nodiscard]] bool isRunning() const;

    // SSE 端点管理
    [[nodiscard]] bool registerSseEndpoint(
        const std::string& endpoint,
        SseEventCallback eventCallback,
        SseConnectionCallback connectionCallback
    );
    [[nodiscard]] bool unregisterSseEndpoint(const std::string& endpoint);
    [[nodiscard]] size_t broadcastSseEvent(const std::string& endpoint, const SseEvent& event);
    [[nodiscard]] bool sendSseEvent(const std::string& endpoint, const std::string& clientId, const SseEvent& event);
    [[nodiscard]] size_t getSseClientCount(const std::string& endpoint) const;
    [[nodiscard]] std::vector<std::string> getSseEndpoints() const;

    // HTTP 路由管理
    [[nodiscard]] bool registerGetHandler(
        const std::string& path,
        std::function<std::string(const std::string&)> handler
    );
    [[nodiscard]] bool registerPostHandler(
        const std::string& path,
        std::function<std::string(const std::string&)> handler
    );
    [[nodiscard]] bool setStaticFileDir(const std::string& mountPoint, const std::string& directory);

    // 状态查询
    [[nodiscard]] std::string getServerAddress() const;
    [[nodiscard]] int getServerPort() const;
    [[nodiscard]] size_t getProcessedRequestCount() const;
    [[nodiscard]] std::string getLastError() const;

    // 配置管理
    void setConfig(const HttpJsonRpcServerConfig& config);
    [[nodiscard]] HttpJsonRpcServerConfig getConfig() const;

private:
    // 初始化服务器
    void setupServer();

    // 设置 CORS 头
    void setCorsHeaders(httplib::Response& res) const;

    // 生成客户端ID
    [[nodiscard]] std::string generateClientId() const;

    // 格式化 SSE 事件
    [[nodiscard]] static std::string formatSseEvent(const SseEvent& event);

    // 发送心跳
    void sendHeartbeat();

private:
    JsonRpc& rpc_;
    HttpJsonRpcServerConfig config_;

    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<std::thread> serverThread_;
    std::atomic<bool> running_;
    std::mutex threadMutex_;  // 保护 serverThread_ 的操作

    // 请求统计
    std::atomic<size_t> processedCount_;

    // 错误信息
    mutable std::mutex errorMutex_;
    std::string lastError_;

    // SSE 连接管理
    struct SseClient {
        std::string clientId;
        std::shared_ptr<std::stringstream> buffer;
        std::mutex mutex;
        bool active;
    };

    struct SseEndpoint {
        SseEventCallback eventCallback;
        SseConnectionCallback connectionCallback;
        std::unordered_map<std::string, std::shared_ptr<SseClient>> clients;
        mutable std::mutex mutex;
    };

    mutable std::mutex sseMutex_;
    std::unordered_map<std::string, std::shared_ptr<SseEndpoint>> sseEndpoints_;

    // 随机数生成器（用于生成客户端ID）
    mutable std::mt19937 rng_;
};

HttpJsonRpcServer::Impl::Impl(JsonRpc& rpc, HttpJsonRpcServerConfig config)
    : rpc_(rpc),
      config_(std::move(config)),
      running_(false),
      processedCount_(0) {
    // 初始化随机数生成器
    std::random_device rd;
    rng_.seed(rd());
}

HttpJsonRpcServer::Impl::~Impl() {
    stop();
}

void HttpJsonRpcServer::Impl::setupServer() {
    server_ = std::make_unique<httplib::Server>();

    // 设置服务器参数
    server_->set_payload_max_length(config_.maxPayloadSize);
    if (config_.threadCount > 0) {
        server_->new_task_queue = [this]() {
            return new httplib::ThreadPool(config_.threadCount);
        };
    }

    // 设置 JSON-RPC 端点
    server_->Post("/", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            processedCount_++;

            // 处理 JSON-RPC 请求
            auto response = rpc_.handleRequest(req.body);

            // 设置响应头
            res.set_header("Content-Type", "application/json");
            setCorsHeaders(res);

            if (response.has_value()) {
                res.set_content(response.value(), "application/json");
            } else {
                // 通知，无响应内容（返回 204 No Content）
                res.status = 204;
            }
        } catch (const SerializationException& e) {
            res.status = 400;
            res.set_content(R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"},"id":null})",
                           "application/json");
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Serialization error: " + std::string(e.what());
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Internal error"},"id":null})",
                           "application/json");
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Internal error: " + std::string(e.what());
        }
    });

    // 支持批量请求
    server_->Post("/rpc", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            processedCount_++;

            auto response = rpc_.handleRequest(req.body);

            res.set_header("Content-Type", "application/json");
            setCorsHeaders(res);

            if (response.has_value()) {
                res.set_content(response.value(), "application/json");
            } else {
                res.status = 204;
            }
        } catch (const SerializationException& e) {
            res.status = 400;
            res.set_content(R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"},"id":null})",
                           "application/json");
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Serialization error: " + std::string(e.what());
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Internal error"},"id":null})",
                           "application/json");
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Internal error: " + std::string(e.what());
        }
    });

    // OPTIONS 请求处理（CORS preflight）
    server_->Options(R"(.*)", [this](const httplib::Request& /*req*/, httplib::Response& res) {
        setCorsHeaders(res);
        res.status = 204;
    });

    // 健康检查端点
    server_->Get("/health", [](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });
}

void HttpJsonRpcServer::Impl::setCorsHeaders(httplib::Response& res) const {
    if (config_.enableCors) {
        res.set_header("Access-Control-Allow-Origin", config_.corsOrigin);
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    }
}

std::string HttpJsonRpcServer::Impl::generateClientId() const {
    std::uniform_int_distribution<> dist(0, 15);
    const char* hex = "0123456789abcdef";

    std::string id;
    id.reserve(32);

    for (int i = 0; i < 32; ++i) {
        id += hex[dist(rng_)];
    }

    return id;
}

std::string HttpJsonRpcServer::Impl::formatSseEvent(const SseEvent& event) {
    std::ostringstream oss;

    if (!event.id.empty()) {
        oss << "id: " << event.id << "\n";
    }

    if (!event.event.empty()) {
        oss << "event: " << event.event << "\n";
    }

    if (event.retry > 0) {
        oss << "retry: " << event.retry << "\n";
    }

    // 处理多行数据
    std::istringstream dataStream(event.data);
    std::string line;
    while (std::getline(dataStream, line)) {
        oss << "data: " << line << "\n";
    }

    oss << "\n";  // 空行表示事件结束

    return oss.str();
}

bool HttpJsonRpcServer::Impl::start() {
    if (running_.load()) {
        return false;
    }

    try {
        setupServer();

        running_.store(true);

        // 阻塞模式启动
        if (!server_->listen(config_.host.c_str(), config_.port)) {
            running_.store(false);
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Failed to bind to " + config_.host + ":" + std::to_string(config_.port);
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        running_.store(false);
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to start server: " + std::string(e.what());
        return false;
    }
}

bool HttpJsonRpcServer::Impl::startAsync() {
    if (running_.load()) {
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(threadMutex_);

        // 只有在 server_ 不存在时才调用 setupServer()
        // 这样可以保留之前通过 registerGetHandler/registerPostHandler 注册的路由
        if (!server_) {
            setupServer();
        }

        running_.store(true);

        serverThread_ = std::make_unique<std::thread>([this]() {
            if (!server_->listen(config_.host.c_str(), config_.port)) {
                running_.store(false);
                std::lock_guard<std::mutex> lock(errorMutex_);
                lastError_ = "Failed to bind to " + config_.host + ":" + std::to_string(config_.port);
            }
        });

        // 等待服务器启动
        int retries = 0;
        while (!server_->is_running() && retries < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            retries++;
        }

        if (!server_->is_running()) {
            stop();
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        running_.store(false);
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to start async server: " + std::string(e.what());
        return false;
    }
}

void HttpJsonRpcServer::Impl::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (server_) {
        server_->stop();
    }

    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        if (serverThread_ && serverThread_->joinable()) {
            serverThread_->join();
        }
        serverThread_.reset();
    }

    server_.reset();

    // 清理所有 SSE 连接
    std::lock_guard<std::mutex> lock(sseMutex_);
    for (auto& [endpoint, ep] : sseEndpoints_) {
        std::lock_guard<std::mutex> epLock(ep->mutex);
        ep->clients.clear();
    }
}

void HttpJsonRpcServer::Impl::wait() {
    std::unique_lock<std::mutex> lock(threadMutex_);
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
        serverThread_.reset();
    }
}

bool HttpJsonRpcServer::Impl::isRunning() const {
    return running_.load() && server_ && server_->is_running();
}

bool HttpJsonRpcServer::Impl::registerSseEndpoint(
    const std::string& endpoint,
    SseEventCallback eventCallback,
    SseConnectionCallback connectionCallback
) {
    if (!config_.enableSse) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "SSE is not enabled";
        return false;
    }

    // 使用 threadMutex_ 保护 setupServer() 调用
    {
        std::lock_guard<std::mutex> lock(threadMutex_);
        if (!server_) {
            setupServer();
        }
    }

    std::lock_guard<std::mutex> lock(sseMutex_);

    // 检查是否已存在
    if (sseEndpoints_.find(endpoint) != sseEndpoints_.end()) {
        return false;
    }

    // 创建端点信息
    auto sseEndpoint = std::make_shared<SseEndpoint>();
    sseEndpoint->eventCallback = std::move(eventCallback);
    sseEndpoint->connectionCallback = std::move(connectionCallback);
    sseEndpoints_[endpoint] = sseEndpoint;

    // 注册 HTTP 端点
    server_->Get(endpoint.c_str(), [this, endpoint](const httplib::Request& req, httplib::Response& res) {
        // 设置 SSE 响应头
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("X-Accel-Buffering", "no");  // 禁用 Nginx 缓冲
        setCorsHeaders(res);

        // 生成客户端 ID
        std::string clientId = generateClientId();

        // 创建客户端连接
        auto client = std::make_shared<SseClient>();
        client->clientId = clientId;
        client->buffer = std::make_shared<std::stringstream>();
        client->active = true;

        // 注册客户端
        {
            std::lock_guard<std::mutex> sseLock(sseMutex_);
            auto it = sseEndpoints_.find(endpoint);
            if (it != sseEndpoints_.end()) {
                std::lock_guard<std::mutex> epLock(it->second->mutex);
                it->second->clients[clientId] = client;

                // 调用连接回调
                if (it->second->connectionCallback) {
                    it->second->connectionCallback(endpoint, clientId, true);
                }
            }
        }

        // 发送初始事件（连接确认）
        SseEvent connectEvent;
        connectEvent.event = "connected";
        connectEvent.data = R"({"clientId":")" + clientId + R"("})";

        {
            std::lock_guard<std::mutex> clientLock(client->mutex);
            *client->buffer << formatSseEvent(connectEvent);
        }

        // 设置分块传输
        res.set_content_provider(
            "text/event-stream",
            [this, endpoint, clientId, client](size_t offset, httplib::DataSink& sink) {
                // 检查客户端是否仍然活跃
                {
                    std::lock_guard<std::mutex> clientLock(client->mutex);
                    if (!client->active) {
                        sink.done();
                        return false;
                    }
                }

                // 读取缓冲区内容并发送
                std::string data;
                {
                    std::lock_guard<std::mutex> clientLock(client->mutex);
                    *client->buffer << std::flush;
                    data = client->buffer->str();
                    if (!data.empty()) {
                        client->buffer->str("");
                        client->buffer->clear();
                    }
                }

                if (!data.empty()) {
                    sink.write(data.c_str(), data.length());
                }

                // 检查 SSE 端点是否存在
                {
                    std::lock_guard<std::mutex> sseLock(sseMutex_);
                    auto it = sseEndpoints_.find(endpoint);
                    if (it == sseEndpoints_.end()) {
                        sink.done();
                        return false;
                    }
                }

                // 短暂休眠避免忙等待
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                return true;
            },
            [this, endpoint, clientId](bool /*success*/) {
                // 连接关闭回调
                std::lock_guard<std::mutex> sseLock(sseMutex_);
                auto it = sseEndpoints_.find(endpoint);
                if (it != sseEndpoints_.end()) {
                    std::lock_guard<std::mutex> epLock(it->second->mutex);
                    it->second->clients.erase(clientId);

                    // 调用断开连接回调
                    if (it->second->connectionCallback) {
                        it->second->connectionCallback(endpoint, clientId, false);
                    }
                }
            }
        );
    });

    return true;
}

bool HttpJsonRpcServer::Impl::unregisterSseEndpoint(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(sseMutex_);

    auto it = sseEndpoints_.find(endpoint);
    if (it == sseEndpoints_.end()) {
        return false;
    }

    // 通知所有客户端断开连接
    {
        std::lock_guard<std::mutex> epLock(it->second->mutex);
        for (auto& [clientId, client] : it->second->clients) {
            std::lock_guard<std::mutex> clientLock(client->mutex);
            client->active = false;
        }
    }

    sseEndpoints_.erase(it);
    return true;
}

size_t HttpJsonRpcServer::Impl::broadcastSseEvent(const std::string& endpoint, const SseEvent& event) {
    std::lock_guard<std::mutex> lock(sseMutex_);

    auto it = sseEndpoints_.find(endpoint);
    if (it == sseEndpoints_.end()) {
        return 0;
    }

    std::string eventStr = formatSseEvent(event);
    size_t count = 0;

    std::lock_guard<std::mutex> epLock(it->second->mutex);
    for (auto& [clientId, client] : it->second->clients) {
        std::lock_guard<std::mutex> clientLock(client->mutex);
        if (client->active) {
            *client->buffer << eventStr;
            count++;

            // 调用事件回调
            if (it->second->eventCallback) {
                it->second->eventCallback(endpoint, clientId, event);
            }
        }
    }

    return count;
}

bool HttpJsonRpcServer::Impl::sendSseEvent(
    const std::string& endpoint,
    const std::string& clientId,
    const SseEvent& event
) {
    std::lock_guard<std::mutex> lock(sseMutex_);

    auto it = sseEndpoints_.find(endpoint);
    if (it == sseEndpoints_.end()) {
        return false;
    }

    std::lock_guard<std::mutex> epLock(it->second->mutex);
    auto clientIt = it->second->clients.find(clientId);
    if (clientIt == it->second->clients.end()) {
        return false;
    }

    std::string eventStr = formatSseEvent(event);

    {
        std::lock_guard<std::mutex> clientLock(clientIt->second->mutex);
        if (!clientIt->second->active) {
            return false;
        }
        *clientIt->second->buffer << eventStr;
    }

    // 调用事件回调
    if (it->second->eventCallback) {
        it->second->eventCallback(endpoint, clientId, event);
    }

    return true;
}

size_t HttpJsonRpcServer::Impl::getSseClientCount(const std::string& endpoint) const {
    std::lock_guard<std::mutex> lock(sseMutex_);

    auto it = sseEndpoints_.find(endpoint);
    if (it == sseEndpoints_.end()) {
        return 0;
    }

    std::lock_guard<std::mutex> epLock(it->second->mutex);
    size_t count = 0;
    for (const auto& [clientId, client] : it->second->clients) {
        std::lock_guard<std::mutex> clientLock(client->mutex);
        if (client->active) {
            count++;
        }
    }
    return count;
}

std::vector<std::string> HttpJsonRpcServer::Impl::getSseEndpoints() const {
    std::lock_guard<std::mutex> lock(sseMutex_);

    std::vector<std::string> endpoints;
    endpoints.reserve(sseEndpoints_.size());

    for (const auto& [endpoint, ep] : sseEndpoints_) {
        endpoints.push_back(endpoint);
    }

    return endpoints;
}

bool HttpJsonRpcServer::Impl::registerGetHandler(
    const std::string& path,
    std::function<std::string(const std::string&)> handler
) {
    std::lock_guard<std::mutex> lock(threadMutex_);
    if (!server_) {
        setupServer();
    }

    server_->Get(path.c_str(), [handler](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string body = req.has_param("q") ? req.get_param_value("q") : req.body;
            std::string response = handler(body);
            res.set_content(response, "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error":")" + std::string(e.what()) + R"("})", "application/json");
        }
    });

    return true;
}

bool HttpJsonRpcServer::Impl::registerPostHandler(
    const std::string& path,
    std::function<std::string(const std::string&)> handler
) {
    std::lock_guard<std::mutex> lock(threadMutex_);
    if (!server_) {
        setupServer();
    }

    server_->Post(path.c_str(), [handler](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string response = handler(req.body);
            res.set_content(response, "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(R"({"error":")" + std::string(e.what()) + R"("})", "application/json");
        }
    });

    return true;
}

bool HttpJsonRpcServer::Impl::setStaticFileDir(const std::string& mountPoint, const std::string& directory) {
    std::lock_guard<std::mutex> lock(threadMutex_);
    if (!server_) {
        setupServer();
    }

    server_->set_mount_point(mountPoint, directory);
    return true;
}

std::string HttpJsonRpcServer::Impl::getServerAddress() const {
    return config_.host;
}

int HttpJsonRpcServer::Impl::getServerPort() const {
    return config_.port;
}

size_t HttpJsonRpcServer::Impl::getProcessedRequestCount() const {
    return processedCount_.load();
}

std::string HttpJsonRpcServer::Impl::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void HttpJsonRpcServer::Impl::setConfig(const HttpJsonRpcServerConfig& config) {
    if (running_.load()) {
        return;  // 不允许在运行时更改配置
    }
    config_ = config;
}

HttpJsonRpcServerConfig HttpJsonRpcServer::Impl::getConfig() const {
    return config_;
}

// ========== HttpJsonRpcServer 公开接口实现 ==========

HttpJsonRpcServer::HttpJsonRpcServer(JsonRpc& rpc, const HttpJsonRpcServerConfig& config)
    : pImpl_(std::make_unique<Impl>(rpc, config)) {
}

HttpJsonRpcServer::~HttpJsonRpcServer() = default;

bool HttpJsonRpcServer::start() {
    return pImpl_->start();
}

bool HttpJsonRpcServer::startAsync() {
    return pImpl_->startAsync();
}

void HttpJsonRpcServer::stop() {
    pImpl_->stop();
}

void HttpJsonRpcServer::wait() {
    pImpl_->wait();
}

bool HttpJsonRpcServer::isRunning() const {
    return pImpl_->isRunning();
}

bool HttpJsonRpcServer::registerSseEndpoint(
    const std::string& endpoint,
    SseEventCallback eventCallback,
    SseConnectionCallback connectionCallback
) {
    return pImpl_->registerSseEndpoint(endpoint, std::move(eventCallback), std::move(connectionCallback));
}

bool HttpJsonRpcServer::unregisterSseEndpoint(const std::string& endpoint) {
    return pImpl_->unregisterSseEndpoint(endpoint);
}

size_t HttpJsonRpcServer::broadcastSseEvent(const std::string& endpoint, const SseEvent& event) {
    return pImpl_->broadcastSseEvent(endpoint, event);
}

bool HttpJsonRpcServer::sendSseEvent(const std::string& endpoint, const std::string& clientId, const SseEvent& event) {
    return pImpl_->sendSseEvent(endpoint, clientId, event);
}

size_t HttpJsonRpcServer::getSseClientCount(const std::string& endpoint) const {
    return pImpl_->getSseClientCount(endpoint);
}

std::vector<std::string> HttpJsonRpcServer::getSseEndpoints() const {
    return pImpl_->getSseEndpoints();
}

bool HttpJsonRpcServer::registerGetHandler(
    const std::string& path,
    std::function<std::string(const std::string&)> handler
) {
    return pImpl_->registerGetHandler(path, std::move(handler));
}

bool HttpJsonRpcServer::registerPostHandler(
    const std::string& path,
    std::function<std::string(const std::string&)> handler
) {
    return pImpl_->registerPostHandler(path, std::move(handler));
}

bool HttpJsonRpcServer::setStaticFileDir(const std::string& mountPoint, const std::string& directory) {
    return pImpl_->setStaticFileDir(mountPoint, directory);
}

std::string HttpJsonRpcServer::getServerAddress() const {
    return pImpl_->getServerAddress();
}

int HttpJsonRpcServer::getServerPort() const {
    return pImpl_->getServerPort();
}

size_t HttpJsonRpcServer::getProcessedRequestCount() const {
    return pImpl_->getProcessedRequestCount();
}

std::string HttpJsonRpcServer::getLastError() const {
    return pImpl_->getLastError();
}

void HttpJsonRpcServer::setConfig(const HttpJsonRpcServerConfig& config) {
    pImpl_->setConfig(config);
}

HttpJsonRpcServerConfig HttpJsonRpcServer::getConfig() const {
    return pImpl_->getConfig();
}

} // namespace mcpserver::json_rpc
