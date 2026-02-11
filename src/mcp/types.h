#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

namespace mcpserver::mcp {

// ========== Tool 相关数据结构 ==========

/**
 * @brief 工具输入Schema（JSON Schema格式）
 */
struct ToolInputSchema {
    std::string type = "object";  // 必须是 "object"
    nlohmann::json properties;    // 参数定义，key -> Schema
    std::vector<std::string> required;  // 必须参数列表

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["type"] = type;
        if (!properties.empty()) {
            j["properties"] = properties;
        }
        if (!required.empty()) {
            j["required"] = required;
        }
        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static ToolInputSchema from_json(const nlohmann::json& j) {
        ToolInputSchema schema;
        if (j.contains("type")) {
            schema.type = j["type"].get<std::string>();
        }
        if (j.contains("properties")) {
            schema.properties = j["properties"];
        }
        if (j.contains("required")) {
            schema.required = j["required"].get<std::vector<std::string>>();
        }
        return schema;
    }
};

/**
 * @brief 内容项类型
 */
enum class ContentType {
    TEXT,
    IMAGE,
    RESOURCE
};

/**
 * @brief 内容项
 */
struct ContentItem {
    std::string type;  // "text", "image", "resource"
    std::optional<std::string> text;       // 文本内容
    std::optional<std::string> data;       // base64编码数据（图片）
    std::optional<std::string> mime_type;  // 媒体类型
    std::optional<std::string> uri;        // 资源URI

    /**
     * @brief 创建文本内容项
     */
    [[nodiscard]] static ContentItem text_content(std::string content) {
        ContentItem item;
        item.type = "text";
        item.text = std::move(content);
        return item;
    }

    /**
     * @brief 创建图片内容项
     */
    [[nodiscard]] static ContentItem image_content(std::string base64_data, std::string mime = "image/png") {
        ContentItem item;
        item.type = "image";
        item.data = std::move(base64_data);
        item.mime_type = std::move(mime);
        return item;
    }

    /**
     * @brief 创建资源内容项
     */
    [[nodiscard]] static ContentItem resource_content(std::string resource_uri) {
        ContentItem item;
        item.type = "resource";
        item.uri = std::move(resource_uri);
        return item;
    }

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["type"] = type;

        if (text.has_value()) {
            j["text"] = text.value();
        }
        if (data.has_value()) {
            j["data"] = data.value();
        }
        if (mime_type.has_value()) {
            j["mime_type"] = mime_type.value();
        }
        if (uri.has_value()) {
            j["uri"] = uri.value();
        }
        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static ContentItem from_json(const nlohmann::json& j) {
        ContentItem item;
        item.type = j["type"].get<std::string>();

        if (j.contains("text")) {
            item.text = j["text"].get<std::string>();
        }
        if (j.contains("data")) {
            item.data = j["data"].get<std::string>();
        }
        if (j.contains("mime_type")) {
            item.mime_type = j["mime_type"].get<std::string>();
        }
        if (j.contains("uri")) {
            item.uri = j["uri"].get<std::string>();
        }
        return item;
    }
};

/**
 * @brief 工具调用结果
 */
struct ToolResult {
    std::vector<ContentItem> content;  // 结果内容（可多个）
    bool is_error = false;             // 是否出错
    std::optional<std::string> error_message;  // 错误消息

    /**
     * @brief 创建成功结果
     */
    [[nodiscard]] static ToolResult success(std::vector<ContentItem> items) {
        ToolResult result;
        result.content = std::move(items);
        result.is_error = false;
        return result;
    }

    /**
     * @brief 创建错误结果
     */
    [[nodiscard]] static ToolResult error(std::string message) {
        ToolResult result;
        result.is_error = true;
        result.error_message = std::move(message);
        return result;
    }

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        nlohmann::json::array_t content_array;

        for (const auto& item : content) {
            content_array.push_back(item.to_json());
        }

        j["content"] = content_array;

        if (is_error) {
            j["isError"] = true;
            if (error_message.has_value()) {
                // 错误消息通常放在第一个content项中
                if (!content.empty()) {
                    // 如果已有内容，修改第一项
                    auto& first = const_cast<ContentItem&>(content.front());
                    if (!first.text.has_value()) {
                        j["content"][0]["text"] = error_message.value();
                    }
                } else {
                    j["content"] = nlohmann::json::array({{
                        {"type", "text"},
                        {"text", error_message.value()}
                    }});
                }
            }
        }

        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static ToolResult from_json(const nlohmann::json& j) {
        ToolResult result;

        if (j.contains("isError")) {
            result.is_error = j["isError"].get<bool>();
        }

        if (j.contains("content") && j["content"].is_array()) {
            for (const auto& item_json : j["content"]) {
                result.content.push_back(ContentItem::from_json(item_json));
            }
        }

        if (j.contains("error_message") || j.contains("errorMessage")) {
            std::string key = j.contains("error_message") ? "error_message" : "errorMessage";
            result.error_message = j[key].get<std::string>();
        }

        return result;
    }
};

/**
 * @brief 工具定义
 */
struct Tool {
    std::string name;                     // 全局唯一，用于调用时指定
    std::string description;              // 人类可读，帮助LLM理解用途
    ToolInputSchema input_schema;         // JSON Schema，定义参数格式

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["name"] = name;
        j["description"] = description;
        j["inputSchema"] = input_schema.to_json();
        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static Tool from_json(const nlohmann::json& j) {
        Tool tool;
        tool.name = j["name"].get<std::string>();
        tool.description = j["description"].get<std::string>();
        if (j.contains("inputSchema")) {
            tool.input_schema = ToolInputSchema::from_json(j["inputSchema"]);
        }
        return tool;
    }
};

// ========== Resource 相关数据结构 ==========

/**
 * @brief 资源定义
 */
struct Resource {
    std::string uri;                       // 唯一资源标识符
    std::string name;                      // 资源显示名称
    std::optional<std::string> description;  // 资源描述
    std::optional<std::string> mime_type;     // MIME类型

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["uri"] = uri;
        j["name"] = name;

        if (description.has_value()) {
            j["description"] = description.value();
        }
        if (mime_type.has_value()) {
            j["mimeType"] = mime_type.value();
        }
        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static Resource from_json(const nlohmann::json& j) {
        Resource resource;
        resource.uri = j["uri"].get<std::string>();
        resource.name = j["name"].get<std::string>();

        if (j.contains("description")) {
            resource.description = j["description"].get<std::string>();
        }
        if (j.contains("mimeType")) {
            resource.mime_type = j["mimeType"].get<std::string>();
        }
        return resource;
    }
};

/**
 * @brief 资源内容
 */
struct ResourceContent {
    std::string uri;                       // 资源URI
    std::optional<std::string> mime_type;  // MIME类型
    std::string text;                      // 文本内容
    std::optional<std::string> blob;       // base64编码的二进制内容

    /**
     * @brief 创建文本资源内容
     */
    [[nodiscard]] static ResourceContent text_resource(std::string resource_uri, std::string content) {
        ResourceContent rc;
        rc.uri = std::move(resource_uri);
        rc.text = std::move(content);
        return rc;
    }

    /**
     * @brief 创建二进制资源内容
     */
    [[nodiscard]] static ResourceContent blob_resource(std::string resource_uri, std::string base64_data,
                                                       std::string mime = "application/octet-stream") {
        ResourceContent rc;
        rc.uri = std::move(resource_uri);
        rc.blob = std::move(base64_data);
        rc.mime_type = std::move(mime);
        return rc;
    }

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["uri"] = uri;

        if (mime_type.has_value()) {
            j["mimeType"] = mime_type.value();
        }

        if (blob.has_value()) {
            j["blob"] = blob.value();
        } else {
            j["text"] = text;
        }

        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static ResourceContent from_json(const nlohmann::json& j) {
        ResourceContent rc;
        rc.uri = j["uri"].get<std::string>();

        if (j.contains("mimeType")) {
            rc.mime_type = j["mimeType"].get<std::string>();
        }

        if (j.contains("blob")) {
            rc.blob = j["blob"].get<std::string>();
        }
        if (j.contains("text")) {
            rc.text = j["text"].get<std::string>();
        }

        return rc;
    }
};

// ========== Prompt 相关数据结构 ==========

/**
 * @brief 角色类型
 */
enum class Role {
    USER,
    ASSISTANT,
    SYSTEM
};

/**
 * @brief 角色转字符串
 */
[[nodiscard]] inline std::string role_to_string(Role role) {
    switch (role) {
        case Role::USER: return "user";
        case Role::ASSISTANT: return "assistant";
        case Role::SYSTEM: return "system";
    }
    return "user";
}

/**
 * @brief 字符串转角色
 */
[[nodiscard]] inline Role string_to_role(const std::string& str) {
    if (str == "user") return Role::USER;
    if (str == "assistant") return Role::ASSISTANT;
    if (str == "system") return Role::SYSTEM;
    return Role::USER;
}

/**
 * @brief 提示参数定义
 */
struct PromptArgument {
    std::string name;                       // 参数名
    std::optional<std::string> description;  // 参数描述
    bool required = false;                  // 是否必须

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["name"] = name;

        if (description.has_value()) {
            j["description"] = description.value();
        }
        if (required) {
            j["required"] = true;
        }

        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static PromptArgument from_json(const nlohmann::json& j) {
        PromptArgument arg;
        arg.name = j["name"].get<std::string>();

        if (j.contains("description")) {
            arg.description = j["description"].get<std::string>();
        }
        if (j.contains("required")) {
            arg.required = j["required"].get<bool>();
        }

        return arg;
    }
};

/**
 * @brief 提示消息
 */
struct PromptMessage {
    Role role;
    nlohmann::json content;  // 可以是字符串或对象

    /**
     * @brief 创建用户文本消息
     */
    [[nodiscard]] static PromptMessage user_text(std::string text) {
        PromptMessage msg;
        msg.role = Role::USER;
        msg.content = std::move(text);
        return msg;
    }

    /**
     * @brief 创建助手消息
     */
    [[nodiscard]] static PromptMessage assistant_text(std::string text) {
        PromptMessage msg;
        msg.role = Role::ASSISTANT;
        msg.content = std::move(text);
        return msg;
    }

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["role"] = role_to_string(role);
        j["content"] = content;
        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static PromptMessage from_json(const nlohmann::json& j) {
        PromptMessage msg;
        msg.role = string_to_role(j["role"].get<std::string>());
        msg.content = j["content"];
        return msg;
    }
};

/**
 * @brief 提示定义
 */
struct Prompt {
    std::string name;                           // 提示名称
    std::optional<std::string> description;      // 提示描述
    std::vector<PromptArgument> arguments;       // 参数列表

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["name"] = name;

        if (description.has_value()) {
            j["description"] = description.value();
        }
        if (!arguments.empty()) {
            nlohmann::json::array_t args_array;
            for (const auto& arg : arguments) {
                args_array.push_back(arg.to_json());
            }
            j["arguments"] = args_array;
        }

        return j;
    }

    /**
     * @brief 从JSON对象构造
     */
    [[nodiscard]] static Prompt from_json(const nlohmann::json& j) {
        Prompt prompt;
        prompt.name = j["name"].get<std::string>();

        if (j.contains("description")) {
            prompt.description = j["description"].get<std::string>();
        }
        if (j.contains("arguments") && j["arguments"].is_array()) {
            for (const auto& arg_json : j["arguments"]) {
                prompt.arguments.push_back(PromptArgument::from_json(arg_json));
            }
        }

        return prompt;
    }
};

/**
 * @brief 提示结果（包含生成的消息列表）
 */
struct PromptResult {
    std::vector<PromptMessage> messages;
    std::optional<std::string> error;  // 错误信息（如果有）

    /**
     * @brief 创建成功结果
     */
    [[nodiscard]] static PromptResult success(std::vector<PromptMessage> msgs) {
        PromptResult result;
        result.messages = std::move(msgs);
        return result;
    }

    /**
     * @brief 创建错误结果
     */
    [[nodiscard]] static PromptResult make_error(std::string message) {
        PromptResult result;
        result.error = std::move(message);
        return result;
    }

    /**
     * @brief 是否有错误
     */
    [[nodiscard]] bool has_error() const {
        return error.has_value();
    }

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        if (error.has_value()) {
            nlohmann::json j;
            j["error"] = error.value();
            return j;
        }

        nlohmann::json::array_t messages_array;
        for (const auto& msg : messages) {
            messages_array.push_back(msg.to_json());
        }

        nlohmann::json j;
        j["messages"] = messages_array;
        return j;
    }
};

// ========== 服务器能力 ==========

/**
 * @brief MCP服务器能力
 */
struct ServerCapabilities {
    bool tools = false;           // 支持工具
    bool resources = false;       // 支持资源
    bool prompts = false;         // 支持提示词

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;

        if (tools) {
            j["tools"] = {};
        }
        if (resources) {
            j["resources"] = {};
        }
        if (prompts) {
            j["prompts"] = {};
        }

        return j;
    }
};

/**
 * @brief 初始化结果
 */
struct InitializeResult {
    ServerCapabilities capabilities;
    std::string server_info;
    std::string version = "1.0.0";

    /**
     * @brief 转换为JSON对象
     */
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["protocolVersion"] = "2024-11-05";
        j["capabilities"] = capabilities.to_json();
        j["serverInfo"] = {
            {"name", server_info},
            {"version", version}
        };
        return j;
    }
};

// ========== 处理器函数类型 ==========

/**
 * @brief 工具调用处理器
 *
 * @param name 工具名称
 * @param arguments 参数（已根据schema验证）
 * @return 工具执行结果
 */
using ToolHandler = std::function<ToolResult(const std::string& name, const nlohmann::json& arguments)>;

/**
 * @brief 资源读取处理器
 *
 * @param uri 资源URI
 * @return 资源内容
 */
using ResourceReadHandler = std::function<ResourceContent(const std::string& uri)>;

/**
 * @brief 提示生成处理器
 *
 * @param name 提示名称
 * @param arguments 参数
 * @return 提示结果
 */
using PromptGetHandler = std::function<PromptResult(const std::string& name,
                                                     const nlohmann::json& arguments)>;

} // namespace mcpserver::mcp
