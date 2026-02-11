#pragma once

#include "types.h"
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <optional>

namespace mcpserver::mcp {

/**
 * @brief 资源注册项
 */
struct ResourceEntry {
    Resource resource;
    ResourceReadHandler read_handler;

    ResourceEntry() = default;

    ResourceEntry(Resource r, ResourceReadHandler h)
        : resource(std::move(r)), read_handler(std::move(h)) {}

    // 支持移动
    ResourceEntry(ResourceEntry&&) noexcept = default;
    ResourceEntry& operator=(ResourceEntry&&) noexcept = default;

    // 禁止拷贝
    ResourceEntry(const ResourceEntry&) = delete;
    ResourceEntry& operator=(const ResourceEntry&) = delete;
};

/**
 * @brief 资源管理器
 *
 * 负责管理MCP服务器的资源注册、注销、列表和读取
 *
 * 功能特性：
 * - 资源注册
 * - 资源注销
 * - 获取资源列表
 * - 读取资源内容
 * - 线程安全
 */
class ResourceManager {
public:
    /**
     * @brief 构造函数
     */
    ResourceManager() = default;

    /**
     * @brief 析构函数
     */
    ~ResourceManager() = default;

    // 禁止拷贝和移动
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // ========== 资源注册 ==========

    /**
     * @brief 注册资源
     *
     * 如果已存在同URI资源，将被覆盖
     *
     * @param resource 资源定义
     * @param handler 资源读取处理器
     * @return 是否注册成功
     */
    bool registerResource(Resource resource, ResourceReadHandler handler);

    /**
     * @brief 注册资源（便捷方法，使用Lambda）
     *
     * @param uri 资源URI
     * @param name 资源显示名称
     * @param description 资源描述（可选）
     * @param mime_type MIME类型（可选）
     * @param handler 资源读取处理器
     * @return 是否注册成功
     */
    bool registerResource(std::string uri, std::string name,
                          std::optional<std::string> description,
                          std::optional<std::string> mime_type,
                          ResourceReadHandler handler);

    /**
     * @brief 注销资源
     *
     * @param uri 资源URI
     * @return 是否注销成功（资源是否存在）
     */
    bool unregisterResource(const std::string& uri);

    /**
     * @brief 检查资源是否已注册
     *
     * @param uri 资源URI
     * @return 是否已注册
     */
    [[nodiscard]] bool hasResource(const std::string& uri) const;

    /**
     * @brief 获取资源定义
     *
     * @param uri 资源URI
     * @return 资源定义（如果存在）
     */
    [[nodiscard]] std::optional<Resource> getResource(const std::string& uri) const;

    // ========== 资源列表 ==========

    /**
     * @brief 获取所有已注册的资源列表
     *
     * @return 资源列表
     */
    [[nodiscard]] std::vector<Resource> listResources() const;

    /**
     * @brief 获取所有已注册的资源URI
     *
     * @return 资源URI列表
     */
    [[nodiscard]] std::vector<std::string> listResourceUris() const;

    /**
     * @brief 获取已注册资源数量
     *
     * @return 资源数量
     */
    [[nodiscard]] size_t getResourceCount() const;

    // ========== 资源读取 ==========

    /**
     * @brief 读取资源内容
     *
     * @param uri 资源URI
     * @return 资源内容
     */
    [[nodiscard]] std::optional<ResourceContent> readResource(const std::string& uri);

    /**
     * @brief 清除所有已注册的资源
     */
    void clearResources();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ResourceEntry> resources_;
};

} // namespace mcpserver::mcp
