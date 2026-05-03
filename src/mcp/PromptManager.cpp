#include "PromptManager.h"
#include "../logger/Logger.h"
#include "../logger/LogMacros.h"

namespace mcpserver::mcp {

bool PromptManager::registerPrompt(Prompt prompt, PromptGetHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto name = prompt.name;
    LOG_INFO("RegisterPrompt called: name='{}', args_count={}", name, prompt.arguments.size());

    prompts_[name] = PromptEntry(std::move(prompt), std::move(handler));

    LOG_INFO("Prompt registered: {}", name);
    return true;
}

bool PromptManager::registerPrompt(std::string name,
                                   std::optional<std::string> description,
                                   std::vector<PromptArgument> arguments,
                                   PromptGetHandler handler) {
    Prompt prompt;
    prompt.name = name;  // 拷贝，不移动
    prompt.description = std::move(description);
    prompt.arguments = std::move(arguments);

    return registerPrompt(std::move(prompt), std::move(handler));
}

bool PromptManager::unregisterPrompt(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = prompts_.find(name);
    if (it == prompts_.end()) {
        LOG_WARN("Attempted to unregister non-existent prompt: {}", name);
        return false;
    }

    prompts_.erase(it);
    LOG_INFO("Prompt unregistered: {}", name);
    return true;
}

bool PromptManager::hasPrompt(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return prompts_.find(name) != prompts_.end();
}

std::optional<Prompt> PromptManager::getPrompt(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = prompts_.find(name);
    if (it == prompts_.end()) {
        return std::nullopt;
    }

    return it->second.prompt;
}

std::vector<Prompt> PromptManager::listPrompts() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Prompt> result;
    result.reserve(prompts_.size());

    for (const auto& [name, entry] : prompts_) {
        result.push_back(entry.prompt);
    }

    return result;
}

std::vector<std::string> PromptManager::listPromptNames() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(prompts_.size());

    for (const auto& [name, entry] : prompts_) {
        result.push_back(name);
    }

    return result;
}

size_t PromptManager::getPromptCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return prompts_.size();
}

std::optional<PromptResult> PromptManager::getPrompt(const std::string& name,
                                                      const nlohmann::json& arguments) {
    LOG_DEBUG("getPrompt called: name='{}', args={}", name, arguments.dump());

    // 先查找提示词，验证参数
    PromptGetHandler handler;
    Prompt prompt_def;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = prompts_.find(name);
        if (it == prompts_.end()) {
            LOG_WARN("Prompt not found: {}", name);
            return std::nullopt;
        }

        prompt_def = it->second.prompt;
        handler = it->second.get_handler;
        LOG_DEBUG("Found prompt: name='{}', args_count={}", prompt_def.name.c_str(), prompt_def.arguments.size());
    }

    // 验证参数
    if (!validateArguments(prompt_def, arguments)) {
        LOG_WARN("Invalid arguments for prompt: {}", name);
        return PromptResult::make_error("Invalid arguments for prompt: " + name);
    }

    // 调用处理器
    try {
        LOG_DEBUG("Calling handler for prompt: {}", name);
        // 确保arguments是一个对象，如果不是则创建空对象
        nlohmann::json args = arguments;
        if (!args.is_object()) {
            LOG_DEBUG("args is not object, converting to empty object");
            args = nlohmann::json::object();
        }
        auto result = handler(name, args);
        LOG_DEBUG("Handler returned, checking result type");
        return result;
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting prompt {}: {}", name, e.what());
        return PromptResult::make_error("Exception in prompt " + name + ": " + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error getting prompt: {}", name);
        return PromptResult::make_error("Unknown error in prompt: " + name);
    }
}

bool PromptManager::validateArguments(const Prompt& prompt, const nlohmann::json& arguments) const {
    // 如果没有参数定义，接受任何参数（或无参数）
    if (prompt.arguments.empty()) {
        return true;
    }

    // Check if arguments is empty/null using explicit type check instead of !
    // This avoids the nlohmann::json operator! which can throw exceptions
    bool is_empty = arguments.is_null() || (arguments.is_object() && arguments.empty());

    // 如果arguments为null或空，检查是否有required参数
    if (is_empty || !arguments.is_object()) {
        for (const auto& arg : prompt.arguments) {
            if (arg.required) {
                return false;  // 缺少必需参数
            }
        }
        return true;
    }

    // 检查所有必需参数是否存在
    for (const auto& arg : prompt.arguments) {
        if (arg.required && !arguments.contains(arg.name)) {
            LOG_WARN("Missing required argument: {} for prompt: {}", arg.name, prompt.name);
            return false;
        }
    }

    return true;
}

void PromptManager::clearPrompts() {
    std::lock_guard<std::mutex> lock(mutex_);
    prompts_.clear();
    LOG_INFO("All prompts cleared");
}

} // namespace mcpserver::mcp
