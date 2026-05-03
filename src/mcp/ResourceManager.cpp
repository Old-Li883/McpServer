#include "ResourceManager.h"
#include "../logger/Logger.h"
#include "../logger/LogMacros.h"

namespace mcpserver::mcp {

bool ResourceManager::registerResource(Resource resource, ResourceReadHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto uri = resource.uri;
    resources_[uri] = ResourceEntry(std::move(resource), std::move(handler));

    LOG_INFO("Resource registered: {}", uri);
    return true;
}

bool ResourceManager::registerResource(std::string uri, std::string name,
                                       std::optional<std::string> description,
                                       std::optional<std::string> mime_type,
                                       ResourceReadHandler handler) {
    Resource resource;
    resource.uri = std::move(uri);
    resource.name = std::move(name);
    resource.description = std::move(description);
    resource.mime_type = std::move(mime_type);

    return registerResource(std::move(resource), std::move(handler));
}

bool ResourceManager::unregisterResource(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = resources_.find(uri);
    if (it == resources_.end()) {
        LOG_WARN("Attempted to unregister non-existent resource: {}", uri);
        return false;
    }

    resources_.erase(it);
    LOG_INFO("Resource unregistered: {}", uri);
    return true;
}

bool ResourceManager::hasResource(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return resources_.find(uri) != resources_.end();
}

std::optional<Resource> ResourceManager::getResource(const std::string& uri) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = resources_.find(uri);
    if (it == resources_.end()) {
        return std::nullopt;
    }

    return it->second.resource;
}

std::vector<Resource> ResourceManager::listResources() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Resource> result;
    result.reserve(resources_.size());

    for (const auto& [uri, entry] : resources_) {
        result.push_back(entry.resource);
    }

    return result;
}

std::vector<std::string> ResourceManager::listResourceUris() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(resources_.size());

    for (const auto& [uri, entry] : resources_) {
        result.push_back(uri);
    }

    return result;
}

size_t ResourceManager::getResourceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return resources_.size();
}

std::optional<ResourceContent> ResourceManager::readResource(const std::string& uri) {
    // 先查找资源，释放锁后再调用handler（避免死锁）
    ResourceReadHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = resources_.find(uri);
        if (it == resources_.end()) {
            LOG_WARN("Resource not found: {}", uri);
            return std::nullopt;
        }

        handler = it->second.read_handler;
    }

    // 调用处理器
    try {
        LOG_DEBUG("Reading resource: {}", uri);
        return handler(uri);
    } catch (const std::exception& e) {
        LOG_ERROR("Error reading resource {}: {}", uri, e.what());
        return std::nullopt;
    } catch (...) {
        LOG_ERROR("Unknown error reading resource: {}", uri);
        return std::nullopt;
    }
}

void ResourceManager::clearResources() {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_.clear();
    LOG_INFO("All resources cleared");
}

} // namespace mcpserver::mcp
