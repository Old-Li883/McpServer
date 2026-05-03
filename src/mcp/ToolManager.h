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
 * @brief 工具注册项
 */
struct ToolEntry {
    Tool tool;
    ToolHandler handler;

    ToolEntry() = default;

    ToolEntry(Tool t, ToolHandler h)
        : tool(std::move(t)), handler(std::move(h)) {}

    // 支持移动
    ToolEntry(ToolEntry&&) noexcept = default;
    ToolEntry& operator=(ToolEntry&&) noexcept = default;

    // 禁止拷贝
    ToolEntry(const ToolEntry&) = delete;
    ToolEntry& operator=(const ToolEntry&) = delete;
};

/**
 * @brief 工具管理器
 *
 * 负责管理MCP服务器的工具注册、注销、列表和调用
 *
 * 功能特性：
 * - 工具注册（支持覆盖同名工具）
 * - 工具注销
 * - 获取工具列表
 * - 调用工具执行
 * - 线程安全
 */
class ToolManager {
public:
    /**
     * @brief 构造函数
     */
    ToolManager() = default;

    /**
     * @brief 析构函数
     */
    ~ToolManager() = default;

    // 禁止拷贝和移动
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;
    ToolManager(ToolManager&&) = delete;
    ToolManager& operator=(ToolManager&&) = delete;

    // ========== 工具注册 ==========

    /**
     * @brief 注册工具
     *
     * 如果已存在同名工具，将被覆盖
     *
     * @param tool 工具定义
     * @param handler 工具调用处理器
     * @return 是否注册成功
     */
    bool registerTool(Tool tool, ToolHandler handler);

    /**
     * @brief 注册工具（便捷方法，使用Lambda）
     *
     * @param name 工具名称
     * @param description 工具描述
     * @param input_schema 输入Schema
     * @param handler 工具调用处理器
     * @return 是否注册成功
     */
    bool registerTool(std::string name, std::string description,
                      ToolInputSchema input_schema, ToolHandler handler);

    /**
     * @brief 注销工具
     *
     * @param name 工具名称
     * @return 是否注销成功（工具是否存在）
     */
    bool unregisterTool(const std::string& name);

    /**
     * @brief 检查工具是否已注册
     *
     * @param name 工具名称
     * @return 是否已注册
     */
    [[nodiscard]] bool hasTool(const std::string& name) const;

    /**
     * @brief 获取工具定义
     *
     * @param name 工具名称
     * @return 工具定义（如果存在）
     */
    [[nodiscard]] std::optional<Tool> getTool(const std::string& name) const;

    // ========== 工具列表 ==========

    /**
     * @brief 获取所有已注册的工具列表
     *
     * @return 工具列表
     */
    [[nodiscard]] std::vector<Tool> listTools() const;

    /**
     * @brief 获取所有已注册的工具名称
     *
     * @return 工具名称列表
     */
    [[nodiscard]] std::vector<std::string> listToolNames() const;

    /**
     * @brief 获取已注册工具数量
     *
     * @return 工具数量
     */
    [[nodiscard]] size_t getToolCount() const;

    // ========== 工具调用 ==========

    /**
     * @brief 调用工具执行
     *
     * @param name 工具名称
     * @param arguments 参数（JSON对象）
     * @return 工具执行结果
     */
    [[nodiscard]] ToolResult callTool(const std::string& name, const nlohmann::json& arguments);

    /**
     * @brief 清除所有已注册的工具
     */
    void clearTools();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolEntry> tools_;
};

} // namespace mcpserver::mcp
