#include "ToolManager.h"
#include "../logger/Logger.h"
#include "../logger/LogMacros.h"

namespace mcpserver::mcp {

bool ToolManager::registerTool(Tool tool, ToolHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto name = tool.name;
    tools_[name] = ToolEntry(std::move(tool), std::move(handler));

    LOG_INFO("Tool registered: {}", name);
    return true;
}

bool ToolManager::registerTool(std::string name, std::string description,
                               ToolInputSchema input_schema, ToolHandler handler) {
    Tool tool;
    tool.name = std::move(name);
    tool.description = std::move(description);
    tool.input_schema = std::move(input_schema);

    return registerTool(std::move(tool), std::move(handler));
}

bool ToolManager::unregisterTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(name);
    if (it == tools_.end()) {
        LOG_WARN("Attempted to unregister non-existent tool: {}", name);
        return false;
    }

    tools_.erase(it);
    LOG_INFO("Tool unregistered: {}", name);
    return true;
}

bool ToolManager::hasTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.find(name) != tools_.end();
}

std::optional<Tool> ToolManager::getTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return std::nullopt;
    }

    return it->second.tool;
}

std::vector<Tool> ToolManager::listTools() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Tool> result;
    result.reserve(tools_.size());

    for (const auto& [name, entry] : tools_) {
        result.push_back(entry.tool);
    }

    return result;
}

std::vector<std::string> ToolManager::listToolNames() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> result;
    result.reserve(tools_.size());

    for (const auto& [name, entry] : tools_) {
        result.push_back(name);
    }

    return result;
}

size_t ToolManager::getToolCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.size();
}

ToolResult ToolManager::callTool(const std::string& name, const nlohmann::json& arguments) {
    // 先查找工具，释放锁后再调用handler（避免死锁）
    ToolHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tools_.find(name);
        if (it == tools_.end()) {
            LOG_WARN("Tool not found: {}", name);
            return ToolResult::error("Tool not found: " + name);
        }

        handler = it->second.handler;
    }

    // 调用处理器
    try {
        LOG_DEBUG("Calling tool: {}", name);
        return handler(name, arguments);
    } catch (const std::exception& e) {
        LOG_ERROR("Error calling tool {}: {}", name, e.what());
        return ToolResult::error("Exception in tool " + name + ": " + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error calling tool: {}", name);
        return ToolResult::error("Unknown error in tool: " + name);
    }
}

void ToolManager::clearTools() {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.clear();
    LOG_INFO("All tools cleared");
}

} // namespace mcpserver::mcp
