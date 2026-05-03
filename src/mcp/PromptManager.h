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
 * @brief 提示词注册项
 */
struct PromptEntry {
    Prompt prompt;
    PromptGetHandler get_handler;

    PromptEntry() = default;

    PromptEntry(Prompt p, PromptGetHandler h)
        : prompt(std::move(p)), get_handler(std::move(h)) {}

    // 支持移动
    PromptEntry(PromptEntry&&) noexcept = default;
    PromptEntry& operator=(PromptEntry&&) noexcept = default;

    // 禁止拷贝
    PromptEntry(const PromptEntry&) = delete;
    PromptEntry& operator=(const PromptEntry&) = delete;
};

/**
 * @brief 提示词管理器
 *
 * 负责管理MCP服务器的提示词模板注册、注销、列表和获取
 *
 * 功能特性：
 * - 提示词模板注册
 * - 提示词模板注销
 * - 获取提示词列表
 * - 获取提示词内容（支持参数替换）
 * - 线程安全
 */
class PromptManager {
public:
    /**
     * @brief 构造函数
     */
    PromptManager() = default;

    /**
     * @brief 析构函数
     */
    ~PromptManager() = default;

    // 禁止拷贝和移动
    PromptManager(const PromptManager&) = delete;
    PromptManager& operator=(const PromptManager&) = delete;
    PromptManager(PromptManager&&) = delete;
    PromptManager& operator=(PromptManager&&) = delete;

    // ========== 提示词注册 ==========

    /**
     * @brief 注册提示词模板
     *
     * 如果已存在同名提示词，将被覆盖
     *
     * @param prompt 提示词定义
     * @param handler 提示词获取处理器
     * @return 是否注册成功
     */
    bool registerPrompt(Prompt prompt, PromptGetHandler handler);

    /**
     * @brief 注册提示词模板（便捷方法，使用Lambda）
     *
     * @param name 提示词名称
     * @param description 提示词描述（可选）
     * @param arguments 参数列表（可选）
     * @param handler 提示词获取处理器
     * @return 是否注册成功
     */
    bool registerPrompt(std::string name,
                        std::optional<std::string> description,
                        std::vector<PromptArgument> arguments,
                        PromptGetHandler handler);

    /**
     * @brief 注销提示词
     *
     * @param name 提示词名称
     * @return 是否注销成功（提示词是否存在）
     */
    bool unregisterPrompt(const std::string& name);

    /**
     * @brief 检查提示词是否已注册
     *
     * @param name 提示词名称
     * @return 是否已注册
     */
    [[nodiscard]] bool hasPrompt(const std::string& name) const;

    /**
     * @brief 获取提示词定义
     *
     * @param name 提示词名称
     * @return 提示词定义（如果存在）
     */
    [[nodiscard]] std::optional<Prompt> getPrompt(const std::string& name) const;

    // ========== 提示词列表 ==========

    /**
     * @brief 获取所有已注册的提示词列表
     *
     * @return 提示词列表
     */
    [[nodiscard]] std::vector<Prompt> listPrompts() const;

    /**
     * @brief 获取所有已注册的提示词名称
     *
     * @return 提示词名称列表
     */
    [[nodiscard]] std::vector<std::string> listPromptNames() const;

    /**
     * @brief 获取已注册提示词数量
     *
     * @return 提示词数量
     */
    [[nodiscard]] size_t getPromptCount() const;

    // ========== 提示词获取 ==========

    /**
     * @brief 获取提示词内容
     *
     * @param name 提示词名称
     * @param arguments 参数（JSON对象，key为参数名）
     * @return 提示词结果
     */
    [[nodiscard]] std::optional<PromptResult> getPrompt(const std::string& name,
                                                         const nlohmann::json& arguments = nullptr);

    /**
     * @brief 清除所有已注册的提示词
     */
    void clearPrompts();

private:
    /**
     * @brief 验证参数是否符合提示词的参数定义
     */
    [[nodiscard]] bool validateArguments(const Prompt& prompt, const nlohmann::json& arguments) const;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, PromptEntry> prompts_;
};

} // namespace mcpserver::mcp
