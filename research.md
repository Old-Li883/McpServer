# MCP Server 深度研究报告

**项目名称**: MCP Server (Model Context Protocol Server)
**语言**: C++17
**协议版本**: MCP 2024-11-05
**报告日期**: 2026-03-01
**作者**: AI Research Analysis

---

## 目录

1. [项目概述](#1-项目概述)
2. [技术架构](#2-技术架构)
3. [核心模块详解](#3-核心模块详解)
4. [关键实现细节](#4-关键实现细节)
5. [协议与数据格式](#5-协议与数据格式)
6. [测试策略](#6-测试策略)
7. [构建与部署](#7-构建与部署)
8. [配置与运维](#8-配置与运维)
9. [文档与图表](#9-文档与图表)
10. [最佳实践](#10-最佳实践)

---

## 1. 项目概述

### 1.1 项目背景

MCP Server 是一个用 C++17 实现的 Model Context Protocol (MCP) 服务器，旨在为 LLM 客户端（如 Claude Desktop）提供标准化的工具、资源和提示词管理能力。该实现遵循 MCP 协议规范（版本 2024-11-05），通过 JSON-RPC 2.0 协议进行通信。

### 1.2 核心特性

| 特性 | 描述 |
|------|------|
| **双模式支持** | 同时支持 STDIO（LSP 格式）和 HTTP 传输 |
| **三大功能** | Tools、Resources、Prompts 管理 |
| **线程安全** | 所有 Manager 类均使用 mutex 保护 |
| **可扩展** | 支持动态注册工具、资源和提示词 |
| **标准协议** | 完全符合 MCP 2024-11-05 规范 |
| **SSE 支持** | HTTP 模式支持 Server-Sent Events 推送 |

### 1.3 技术栈

```
核心依赖:
├── nlohmann/json  3.11.3   # JSON 序列化
├── spdlog          1.12.0   # 日志框架
├── fmt             12.1.0   # 格式化库
├── cpp-httplib     0.15.3   # HTTP 服务器
└── Google Test     1.14.0   # 单元测试

编译要求:
├── CMake           3.15+
└── C++             C++17
```

### 1.4 项目结构

```
mcpserver/
├── src/
│   ├── main.cpp              # 入口点，包含示例 tools/resources/prompts
│   ├── logger/               # 日志子系统 (spdlog)
│   ├── json_rpc/             # JSON-RPC 2.0 实现
│   │   ├── stdio_jsonrpc     # STDIO 传输 (LSP 格式)
│   │   └── http_jsonrpc_server  # HTTP 服务器
│   └── mcp/                  # MCP 协议实现
│       ├── McpServer         # 核心服务器
│       ├── McpServerRunner   # 生命周期管理
│       ├── ToolManager       # 工具管理
│       ├── ResourceManager   # 资源管理
│       └── PromptManager     # 提示管理
├── config/                   # 配置管理
├── tests/                    # Google Test 测试
├── docs/                     # PlantUML 图表
└── CMakeLists.txt            # 构建配置
```

---

## 2. 技术架构

### 2.1 整体架构（5层设计）

```
┌─────────────────────────────────────────────────────────┐
│                    客户层 (Client Layer)                  │
│  Claude Desktop | Web Client | Custom MCP Client        │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│                  传输层 (Transport Layer)                 │
│  StdioJsonRpc (LSP Format) | HttpJsonRpcServer (SSE)   │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│                  协议层 (Protocol Layer)                  │
│  JsonRpc (JSON-RPC 2.0) | McpServer (MCP Method Router) │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│                   业务层 (Business Layer)                 │
│  ToolManager | ResourceManager | PromptManager         │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│                    数据层 (Data Layer)                    │
│  types.h (Serialization) | Config | Logger             │
└─────────────────────────────────────────────────────────┘
```

### 2.2 分层设计说明

| 层级 | 职责 | 关键组件 |
|------|------|----------|
| **客户端层** | 外部 LLM 客户端 | Claude Desktop, Web Client |
| **传输层** | 消息传输协议适配 | StdioJsonRpc, HttpJsonRpcServer |
| **协议层** | JSON-RPC 路由与 MCP 方法分发 | JsonRpc, McpServer |
| **业务层** | 工具/资源/提示词管理 | ToolManager, ResourceManager, PromptManager |
| **数据层** | 数据结构、配置、日志 | types.h, Config, Logger |

### 2.3 设计模式应用

| 设计模式 | 应用位置 | 说明 |
|----------|----------|------|
| **Singleton** | Config, Logger | 全局唯一实例，Meyer's 实现 |
| **Facade** | McpServerRunner | 隐藏复杂的初始化和生命周期管理 |
| **Strategy** | 传输层抽象 | HTTP/STDIO 可互换 |
| **Pimpl** | HttpJsonRpcServer | 隐藏 cpp-httplib 实现细节 |
| **Observer** | SSE 事件回调 | 向客户端推送实时事件 |

---

## 3. 核心模块详解

### 3.1 MCP 协议层 (McpServer)

**文件位置**: `src/mcp/McpServer.h`, `src/mcp/McpServer.cpp`

McpServer 是 MCP 协议的核心实现类，负责三大功能的统一管理和方法路由。

```cpp
class McpServer {
public:
    // 工具管理代理方法
    bool registerTool(Tool tool, ToolHandler handler);
    bool unregisterTool(const std::string& name);
    std::vector<Tool> listTools() const;

    // 资源管理代理方法
    bool registerResource(Resource resource, ResourceReadHandler handler);
    bool unregisterResource(const std::string& uri);
    std::vector<Resource> listResources() const;

    // 提示管理代理方法
    bool registerPrompt(Prompt prompt, PromptGetHandler handler);
    bool unregisterPrompt(const std::string& name);
    std::vector<Prompt> listPrompts() const;

    // JSON-RPC 集成
    void registerMethods(json_rpc::JsonRpc& jsonRpc);

    // SSE 事件系统
    void setSseEventCallback(SseEventCallback callback);
    void sendSseEvent(const std::string& event, const nlohmann::json& data);
};
```

**支持的 MCP 方法**:

| 方法 | 功能 | 参数 | 返回值 |
|------|------|------|--------|
| `initialize` | 协议初始化 | - | ServerCapabilities |
| `tools/list` | 获取工具列表 | - | Tool[] |
| `tools/call` | 调用工具 | name, arguments | ToolResult |
| `resources/list` | 获取资源列表 | - | Resource[] |
| `resources/read` | 读取资源内容 | uri | ResourceContent |
| `prompts/list` | 获取提示词列表 | - | Prompt[] |
| `prompts/get` | 获取提示词内容 | name, arguments | PromptResult |

### 3.2 JSON-RPC 层 (JsonRpc)

**文件位置**: `src/json_rpc/jsonrpc.h`, `src/json_rpc/jsonrpc.cpp`

完整的 JSON-RPC 2.0 实现，支持请求/响应、批量请求、通知等。

```cpp
class JsonRpc {
public:
    // 方法注册
    bool registerMethod(const std::string& method, MethodHandler handler);
    bool registerAsyncMethod(const std::string& method, AsyncMethodHandler handler);
    bool unregisterMethod(const std::string& method);

    // 请求处理
    std::optional<Response> handleRequest(const Request& request);
    std::optional<BatchResponse> handleBatchRequest(const BatchRequest& requests);
    std::optional<std::string> handleRequest(const std::string& jsonStr);

    // 错误处理
    void setExceptionHandler(std::function<void(const std::exception&)> handler);
    void setMethodNotFoundHandler(Response (*handler)(const std::string&, const nlohmann::json&));
};
```

**错误码定义**:

```cpp
enum class ErrorCode {
    PARSE_ERROR = -32700,           // JSON 解析错误
    INVALID_REQUEST = -32600,       // 无效请求
    METHOD_NOT_FOUND = -32601,      // 方法未找到
    INVALID_PARAMS = -32602,        // 无效参数
    INTERNAL_ERROR = -32603,        // 内部错误
    SERVER_ERROR_START = -32099,    // 服务器错误范围
    SERVER_ERROR_END = -32000,
};
```

### 3.3 传输层 (Transport Layer)

#### 3.3.1 STDIO 传输 (StdioJsonRpc)

**文件位置**: `src/json_rpc/stdio_jsonrpc.h`

用于 Claude Desktop 等需要标准输入输出通信的场景。

```cpp
class StdioJsonRpc {
public:
    // LSP/MCP 格式消息读取
    bool readAndProcess();
    bool sendMessage(const std::string& message);

    // 异步操作
    bool start();
    void stop();

    // 回调设置
    void setRequestCallback(MessageCallback callback);
    void setResponseCallback(MessageCallback callback);
};
```

**LSP 消息格式**:
```
Content-Length: 123\r\n
\r\n
{"jsonrpc":"2.0","method":"tools/list",...}
```

#### 3.3.2 HTTP 传输 (HttpJsonRpcServer)

**文件位置**: `src/json_rpc/http_jsonrpc_server.h`

用于 Web 服务场景，支持 SSE 推送。

```cpp
class HttpJsonRpcServer {
public:
    // 服务器控制
    bool start();                    // 阻塞启动
    bool startAsync();               // 异步启动
    void stop();

    // SSE 端点管理
    bool registerSseEndpoint(const std::string& endpoint,
                            SseEventCallback eventCallback,
                            SseConnectionCallback connectionCallback = nullptr);
    size_t broadcastSseEvent(const std::string& endpoint, const SseEvent& event);

    // HTTP 路由
    bool registerGetHandler(const std::string& path,
                           std::function<std::string(const std::string&)> handler);
    bool registerPostHandler(const std::string& path,
                            std::function<std::string(const std::string&)> handler);
};
```

### 3.4 管理器层 (Manager Layer)

#### 3.4.1 ToolManager

**文件位置**: `src/mcp/ToolManager.h`

```cpp
class ToolManager {
public:
    bool registerTool(Tool tool, ToolHandler handler);
    bool unregisterTool(const std::string& name);
    std::vector<Tool> listTools() const;
    ToolResult callTool(const std::string& name, const nlohmann::json& arguments);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolEntry> tools_;
};
```

**工具执行流程**:
```
1. 加锁查找工具
2. 验证参数符合 Schema
3. 调用用户定义的 handler
4. 返回 ToolResult
```

#### 3.4.2 ResourceManager

**文件位置**: `src/mcp/ResourceManager.h`

```cpp
class ResourceManager {
public:
    bool registerResource(Resource resource, ResourceReadHandler handler);
    bool unregisterResource(const std::string& uri);
    std::vector<Resource> listResources() const;
    std::optional<ResourceContent> readResource(const std::string& uri);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ResourceEntry> resources_;
};
```

#### 3.4.3 PromptManager

**文件位置**: `src/mcp/PromptManager.h`

```cpp
class PromptManager {
public:
    bool registerPrompt(Prompt prompt, PromptGetHandler handler);
    bool unregisterPrompt(const std::string& name);
    std::vector<Prompt> listPrompts() const;
    std::optional<PromptResult> getPrompt(const std::string& name,
                                         const nlohmann::json& arguments = nullptr);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PromptEntry> prompts_;
};
```

### 3.5 生命周期管理 (McpServerRunner)

**文件位置**: `src/mcp/McpServerRunner.h`

Facade 模式的统一入口，管理整个服务器的生命周期。

```cpp
class McpServerRunner {
public:
    // 便捷注册方法（代理到 McpServer）
    bool registerTool(std::string name, std::string description,
                     ToolInputSchema input_schema, ToolHandler handler);
    bool registerResource(std::string uri, std::string name,
                         std::optional<std::string> description,
                         std::optional<std::string> mime_type,
                         ResourceReadHandler handler);
    bool registerPrompt(std::string name,
                       std::optional<std::string> description,
                       std::vector<PromptArgument> arguments,
                       PromptGetHandler handler);

    // 服务器控制
    bool run();          // 阻塞运行
    bool startAsync();   // 异步启动
    void stop();
    void wait();

    // 状态查询
    bool isRunning() const;
    ServerMode getMode() const;
    std::string getServerAddress() const;
    int getServerPort() const;
};
```

**运行模式**:
```cpp
enum class ServerMode {
    HTTP,   // HTTP 模式
    STDIO,  // STDIO 模式
    BOTH,   // 同时运行 HTTP 和 STDIO
};
```

---

## 4. 关键实现细节

### 4.1 线程安全机制

#### 4.1.1 Manager 类的线程安全

所有 Manager 类都使用 `std::mutex` 保护内部数据结构：

```cpp
class ToolManager {
private:
    mutable std::mutex mutex_;  // mutable 允许 const 方法加锁
    std::unordered_map<std::string, ToolEntry> tools_;

public:
    std::vector<Tool> listTools() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Tool> result;
        for (const auto& [name, entry] : tools_) {
            result.push_back(entry.tool);
        }
        return result;
    }
};
```

#### 4.1.2 原子操作

运行状态和计数器使用 `std::atomic`：

```cpp
class McpServerRunner {
private:
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> httpThread_;
};
```

#### 4.1.3 条件变量

StdioJsonRpc 使用条件变量进行消息队列同步：

```cpp
class StdioJsonRpc {
private:
    std::queue<std::string> messageQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
};
```

### 4.2 内存管理 (RAII)

#### 4.2.1 Pimpl 模式

HttpJsonRpcServer 使用 Pimpl 隐藏实现：

```cpp
class HttpJsonRpcServer {
private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;  // 不完整类型，减少编译依赖
};
```

#### 4.2.2 智能指针使用

```cpp
class McpServer {
private:
    std::unique_ptr<ToolManager> tool_manager_;
    std::unique_ptr<ResourceManager> resource_manager_;
    std::unique_ptr<PromptManager> prompt_manager_;
};
```

#### 4.2.3 移动语义优化

ToolEntry 等结构体支持移动：

```cpp
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
```

### 4.3 错误处理

#### 4.3.1 异常安全

JSON-RPC 处理器捕获所有异常并转换为错误响应：

```cpp
Response JsonRpc::processRequest(const Request& request) {
    try {
        auto result = invokeMethod(request.method, request.params);
        if (result.has_value()) {
            return JsonRpcSerialization::createSuccessResponse(result.value(), request.id.value());
        }
    } catch (const std::exception& e) {
        if (exceptionHandler_) {
            exceptionHandler_(e);
        }
        return createErrorResponse(ErrorCode::INTERNAL_ERROR, request.id.value());
    }
}
```

#### 4.3.2 Optional 返回值

使用 `std::optional` 表示可能失败的操作：

```cpp
std::optional<ResourceContent> ResourceManager::readResource(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = resources_.find(uri);
    if (it == resources_.end()) {
        return std::nullopt;  // 资源不存在
    }
    return it->second.read_handler(uri);
}
```

#### 4.3.3 自定义异常

```cpp
class SerializationException : public std::exception {
public:
    explicit SerializationException(const std::string& message) : message_(message) {}
    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }
private:
    std::string message_;
};
```

### 4.4 性能优化

#### 4.4.1 避免不必要的拷贝

```cpp
// 使用 const引用避免拷贝
bool registerTool(std::string name, std::string description,
                 ToolInputSchema input_schema, ToolHandler handler);

// 返回值优化 (RVO)
std::vector<Tool> listTools() const;
```

#### 4.4.2 Lambda 捕获优化

```cpp
runner.registerTool("echo", "Echo back the input",
    ToolInputSchema{{}, {}},
    [](const std::string& name, const nlohmann::json& args) -> ToolResult {
        std::string text = args.value("text", "");
        return ToolResult::success({ContentItem::text_content("Echo: " + text)});
    }
);
```

#### 4.4.3 日志条件编译

```cpp
#ifdef DEBUG
    LOG_DEBUG("Processing request: {}", request_str);
#endif
```

---

## 5. 协议与数据格式

### 5.1 MCP 方法详解

#### 5.1.1 initialize

**请求**:
```json
{
  "jsonrpc": "2.0",
  "method": "initialize",
  "params": {
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "clientInfo": {
      "name": "my-client",
      "version": "1.0.0"
    }
  },
  "id": 1
}
```

**响应**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {
      "tools": {},
      "resources": {},
      "prompts": {}
    },
    "serverInfo": {
      "name": "mcp-server",
      "version": "1.0.0"
    }
  },
  "id": 1
}
```

#### 5.1.2 tools/list

**请求**:
```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "id": 2
}
```

**响应**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "tools": [
      {
        "name": "echo",
        "description": "Echo back the input text",
        "inputSchema": {
          "type": "object",
          "properties": {
            "text": {
              "type": "string",
              "description": "The text to echo back"
            }
          },
          "required": ["text"]
        }
      }
    ]
  },
  "id": 2
}
```

#### 5.1.3 tools/call

**请求**:
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "echo",
    "arguments": {
      "text": "Hello, MCP!"
    }
  },
  "id": 3
}
```

**响应**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Echo: Hello, MCP!"
      }
    ]
  },
  "id": 3
}
```

### 5.2 JSON-RPC 消息格式

#### 5.2.1 请求对象

```cpp
struct Request {
    std::string jsonrpc = "2.0";
    std::string method;
    nlohmann::json params;           // 可选: array 或 object
    std::optional<nlohmann::json> id; // 可选: string, number, or null

    [[nodiscard]] bool isNotification() const { return !id.has_value(); }
};
```

#### 5.2.2 响应对象

```cpp
struct Response {
    std::string jsonrpc = "2.0";
    std::optional<nlohmann::json> result;
    std::optional<Error> error;
    nlohmann::json id;

    [[nodiscard]] bool isSuccess() const { return result.has_value() && !error.has_value(); }
    [[nodiscard]] bool isError() const { return error.has_value(); }
};
```

#### 5.2.3 错误对象

```cpp
struct Error {
    int code;
    std::string message;
    std::optional<nlohmann::json> data;
};
```

### 5.3 LSP 消息格式

用于 STDIO 模式的消息封装：

```
Content-Length: <length>\r\n
\r\n
<JSON-RPC message>
```

示例：
```
Content-Length: 87\r\n
\r\n
{"jsonrpc":"2.0","method":"tools/list","id":1}
```

### 5.4 数据结构定义

**ToolInputSchema** (JSON Schema):
```cpp
struct ToolInputSchema {
    std::string type = "object";
    nlohmann::json properties;
    std::vector<std::string> required;
};
```

**ContentItem**:
```cpp
struct ContentItem {
    std::string type;  // "text", "image", "resource"
    std::optional<std::string> text;
    std::optional<std::string> data;       // base64 编码
    std::optional<std::string> mime_type;
    std::optional<std::string> uri;

    static ContentItem text_content(std::string content);
    static ContentItem image_content(std::string base64_data, std::string mime = "image/png");
    static ContentItem resource_content(std::string resource_uri);
};
```

---

## 6. 测试策略

### 6.1 测试结构

```
tests/
├── test_logger.cpp              # 日志系统测试
├── test_jsonrpc.cpp             # JSON-RPC 核心测试
├── test_stdio_jsonrpc.cpp       # STDIO 传输测试
├── test_http_jsonrpc_server.cpp # HTTP 服务器测试
└── test_mcp_server.cpp          # MCP 服务器集成测试
```

### 6.2 CMake 测试配置

```cmake
# 日志测试
add_executable(test_logger tests/test_logger.cpp src/logger/Logger.cpp)
target_link_libraries(test_logger PRIVATE ${SPDLOG_TARGET})
link_gtest(test_logger)

# JSON-RPC 测试
add_executable(test_jsonrpc tests/test_jsonrpc.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
)
target_link_libraries(test_jsonrpc PRIVATE nlohmann_json::nlohmann_json)
link_gtest(test_jsonrpc)

# MCP 服务器测试
add_executable(test_mcp_server tests/test_mcp_server.cpp
    src/json_rpc/jsonrpc_serialization.cpp
    src/json_rpc/jsonrpc.cpp
    src/mcp/ToolManager.cpp
    src/mcp/ResourceManager.cpp
    src/mcp/PromptManager.cpp
    src/mcp/McpServer.cpp
    src/logger/Logger.cpp
)
target_link_libraries(test_mcp_server PRIVATE
    nlohmann_json::nlohmann_json
    ${SPDLOG_TARGET}
)
link_gtest(test_mcp_server)

# 自动发现测试
gtest_discover_tests(test_logger)
gtest_discover_tests(test_jsonrpc)
gtest_discover_tests(test_mcp_server)
```

### 6.3 测试覆盖

| 模块 | 测试内容 |
|------|----------|
| Logger | 日志级别、文件轮转、多线程安全性 |
| JsonRpc | 请求解析、批量请求、错误处理 |
| StdioJsonRpc | LSP 格式解析、消息队列 |
| HttpJsonRpcServer | HTTP 路由、SSE 推送 |
| McpServer | 方法路由、Manager 集成 |

### 6.4 运行测试

```bash
# 构建测试
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 运行所有测试
cd build
ctest --output-on-failure

# 运行特定测试
./test_logger
./test_jsonrpc
./test_mcp_server
```

---

## 7. 构建与部署

### 7.1 CMake 配置

**CMakeLists.txt 关键配置**:

```cmake
cmake_minimum_required(VERSION 3.15)
project(McpServer)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖管理
include(FetchContent)

# nlohmann/json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

# spdlog
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(spdlog_fc
        URL https://github.com/gabime/spdlog/releases/download/v1.12.0/spdlog-1.12.0.tgz
    )
    FetchContent_MakeAvailable(spdlog_fc)
endif()

# cpp-httplib
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    FetchContent_Declare(httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.15.3
    )
    FetchContent_MakeAvailable(httplib)
endif()
```

### 7.2 构建步骤

```bash
# 配置
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build -j$(nproc)

# 运行
./build/McpServer
```

### 7.3 依赖版本控制

| 依赖 | 版本 | 获取方式 |
|------|------|----------|
| nlohmann/json | 3.11.3 | FetchContent |
| spdlog | 1.12.0 | FetchContent |
| fmt | 12.1.0 | FetchContent |
| cpp-httplib | 0.15.3 | FetchContent |
| Google Test | 1.14.0 | FetchContent |

### 7.4 跨平台支持

- **macOS**: ✅ 支持（使用 Homebrew 或 FetchContent）
- **Linux**: ✅ 支持（使用系统包或 FetchContent）
- **Windows**: ⚠️ 需要额外配置（MinGW 或 MSVC）

---

## 8. 配置与运维

### 8.1 配置文件 (config.json)

```json
{
  "server": {
    "mode": "both",
    "port": 8080,
    "host": "0.0.0.0"
  },
  "logging": {
    "level": "info",
    "filePath": "logs/mcpserver.log",
    "fileSize": 5242880,
    "fileCount": 3,
    "consoleOutput": true
  }
}
```

### 8.2 配置类 (Config)

**文件位置**: `config/Config.h`

```cpp
class Config {
public:
    static Config& getInstance();  // Meyer's Singleton

    // Server 配置
    int getServerPort() const;
    std::string getServerMode() const;

    // Logging 配置
    std::string getLogLevel() const;
    std::string getLogFilePath() const;
    int getLogFileSize() const;
    int getLogFileCount() const;
    bool getLogConsoleOutput() const;

    // 文件操作
    bool loadFromFile(const std::string& filePath);
    bool saveToFile(const std::string& filePath);
};
```

### 8.3 日志系统 (Logger)

**文件位置**: `src/logger/Logger.h`

```cpp
class Logger {
public:
    static Logger& getInstance();

    bool initialize(const std::string& logFilePath = "logs/mcpserver.log",
                   const std::string& logLevel = "info",
                   size_t maxFileSize = 5 * 1024 * 1024,
                   size_t maxFiles = 3,
                   bool enableConsole = true);

    // 日志记录
    template<typename... Args>
    void info(const std::string& fmt, Args&&... args);
    template<typename... Args>
    void error(const std::string& fmt, Args&&... args);
    // ... debug, warn, critical, trace

    // 日志控制
    void setLogLevel(LogLevel level);
    void flush();
    void shutdown();
};
```

**日志级别**:
- TRACE < DEBUG < INFO < WARN < ERROR < CRITICAL < OFF

**日志宏** (LogMacros.h):
```cpp
#define LOG_TRACE(logger, ...) logger.trace(__VA_ARGS__)
#define LOG_DEBUG(logger, ...) logger.debug(__VA_ARGS__)
#define LOG_INFO(logger, ...) logger.info(__VA_ARGS__)
#define LOG_WARN(logger, ...) logger.warn(__VA_ARGS__)
#define LOG_ERROR(logger, ...) logger.error(__VA_ARGS__)
#define LOG_CRITICAL(logger, ...) logger.critical(__VA_ARGS__)

// 使用默认 Logger
#define LOG_INFO(...) mcpserver::Logger::getInstance().info(__VA_ARGS__)
```

### 8.4 运行模式切换

**main.cpp 中的模式配置**:

```cpp
Config& config = Config::getInstance();
config.loadFromFile("config.json");

McpServerRunnerConfig runnerConfig;
runnerConfig.mode = stringToServerMode(config.getServerMode());
// ServerMode::HTTP | ServerMode::STDIO | ServerMode::BOTH

runnerConfig.host = "0.0.0.0";
runnerConfig.port = config.getServerPort();
runnerConfig.useLspFormat = true;

McpServerRunner runner(runnerConfig);
runner.run();  // 阻塞运行
```

---

## 9. 文档与图表

### 9.1 PlantUML 图表

项目包含 7 个 PlantUML 架构图：

| 图表 | 文件 | 描述 |
|------|------|------|
| 逻辑视图 | `01_logical_view.puml` | 5层架构设计 |
| 进程视图 | `02_process_view.puml` | 启动流程、并发模型 |
| 实现视图 | `03_implementation_view.puml` | 目录结构、模块依赖 |
| 部署视图 | `04_deployment_view.puml` | 部署配置、运行环境 |
| 用例视图 | `05_usecase_view.puml` | 用户场景、功能边界 |
| 时序图 | `06_sequence_diagram.puml` | 端到端交互流程 |
| 流程图 | `07_flow_diagram.puml` | 数据流向、处理步骤 |

### 9.2 生成图表

```bash
# 安装 PlantUML
brew install plantuml  # macOS
apt-get install plantuml  # Ubuntu

# 生成 PNG
plantuml docs/*.puml

# 生成 SVG
plantuml -tsvg docs/*.puml
```

### 9.3 架构概览

```
┌───────────────────────────────────────────────────────────────┐
│                        逻辑视图 (5层)                           │
├───────────────────────────────────────────────────────────────┤
│ 客户层      │ Claude Desktop │ Web Client │ Custom Client     │
├───────────────────────────────────────────────────────────────┤
│ 传输层      │ StdioJsonRpc (LSP) │ HttpJsonRpcServer (SSE)   │
├───────────────────────────────────────────────────────────────┤
│ 协议层      │ JsonRpc (JSON-RPC 2.0) │ McpServer (MCP)        │
├───────────────────────────────────────────────────────────────┤
│ 业务层      │ ToolManager │ ResourceManager │ PromptManager  │
├───────────────────────────────────────────────────────────────┤
│ 数据层      │ types.h │ Config │ Logger                       │
└───────────────────────────────────────────────────────────────┘
```

---

## 10. 最佳实践

### 10.1 C++17 特性使用

| 特性 | 应用位置 | 示例 |
|------|----------|------|
| `std::optional` | 可选返回值 | `std::optional<Tool> getTool(const std::string& name)` |
| `std::nullopt` | Optional 空值 | `return std::nullopt;` |
| `[[nodiscard]]` | 强制检查返回值 | `[[nodiscard]] bool registerTool(...)` |
| 结构化绑定 | 遍历 map | `for (const auto& [name, entry] : tools_)` |
| `std::string_view` | 避免字符串拷贝 | (当前未使用，可优化) |
| `if constexpr` | 编译时条件 | (可用于模板优化) |
| 折叠表达式 | 参数包展开 | (日志系统可优化) |

### 10.2 设计原则应用

| 原则 | 应用 | 说明 |
|------|------|------|
| **SOLID** | 接口隔离 | Manager 类各司其职 |
| **DRY** | 代码复用 | types.h 统一数据结构 |
| **RAII** | 资源管理 | 智能指针管理生命周期 |
| **Rule of Zero** | 自动生成 | ToolEntry 支持移动，禁用拷贝 |
| **Pimpl** | 隐藏实现 | HttpJsonRpcServer |

### 10.3 代码规范

#### 命名约定

```cpp
// 类名: PascalCase
class McpServer {};
class ToolManager {};

// 方法名: camelCase
bool registerTool();
std::vector<Tool> listTools() const;

// 成员变量: camelCase + 下划线后缀
std::unique_ptr<ToolManager> tool_manager_;
mutable std::mutex mutex_;

// 常量: UPPER_CASE
static constexpr const char* DEFAULT_CONSOLE_PATTERN = "...";

// 枚举: PascalCase
enum class ServerMode { HTTP, STDIO, BOTH };
```

#### 注释风格

```cpp
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
```

### 10.4 性能建议

1. **避免过早优化**: 先保证正确性，再优化性能
2. **使用移动语义**: 大对象传递使用 `std::move`
3. **减少锁粒度**: 只在必要时加锁，尽快释放
4. **避免不必要拷贝**: 使用 `const&` 传递参数
5. **日志条件判断**: 生产环境关闭 DEBUG 日志

### 10.5 安全建议

1. **输入验证**: 验证所有外部输入
2. **错误处理**: 不泄露敏感信息
3. **资源限制**: 限制消息大小、连接数
4. **日志脱敏**: 不记录敏感数据
5. **依赖审计**: 定期更新依赖库

---

## 附录

### A. 示例代码

#### A.1 创建简单工具

```cpp
runner.registerTool(
    "get_current_time",
    "Get the current date and time",
    ToolInputSchema{
        {"type", "object"},
        {{"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}}
    },
    [](const std::string& name, const nlohmann::json& args) -> ToolResult {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string timeStr = std::ctime(&time);
        if (!timeStr.empty() && timeStr.back() == '\n') {
            timeStr.pop_back();
        }
        return ToolResult::success({
            ContentItem::text_content("Current time: " + timeStr)
        });
    }
);
```

#### A.2 创建资源

```cpp
runner.registerResource(
    "system://info",
    "System Information",
    "Basic system information",
    "text/plain",
    [](const std::string& uri) -> ResourceContent {
        std::string info = R"({
  "server": "MCP Server",
  "version": "1.0.0",
  "status": "running"
})";
        return ResourceContent::text_resource(uri, info);
    }
);
```

#### A.3 创建提示词

```cpp
runner.registerPrompt(
    "summarize",
    "Summarize the given text in a concise manner",
    {
        {"text", "The text to summarize", false}
    },
    [](const std::string& name, const nlohmann::json& args) -> PromptResult {
        std::string text = args.value("text", "");
        std::string summary = "Please summarize the following text:\n\n" + text;
        return PromptResult::success({
            PromptMessage::user_text(summary)
        });
    }
);
```

### B. MCP 方法参考表

| 方法 | 请求参数 | 响应结果 | 错误码 |
|------|----------|----------|--------|
| initialize | clientInfo | ServerCapabilities | - |
| tools/list | - | tools[] | - |
| tools/call | name, arguments | content[] | InvalidParams |
| resources/list | - | resources[] | - |
| resources/read | uri | content | InvalidParams |
| prompts/list | - | prompts[] | - |
| prompts/get | name, arguments | messages[] | InvalidParams |

### C. 错误码快速参考

| 代码 | 名称 | 说明 |
|------|------|------|
| -32700 | ParseError | JSON 解析失败 |
| -32600 | InvalidRequest | 请求格式错误 |
| -32601 | MethodNotFound | 方法不存在 |
| -32602 | InvalidParams | 参数验证失败 |
| -32603 | InternalError | 服务器内部错误 |
| -32099 ~ -32000 | ServerError | 服务器错误范围 |

### D. 参考资料

- [MCP 规范 (2024-11-05)](https://spec.modelcontextprotocol.io/specification/)
- [JSON-RPC 2.0 规范](https://www.jsonrpc.org/specification)
- [LSP (Language Server Protocol)](https://microsoft.github.io/language-server-protocol/)
- [cpp-httplib 文档](https://github.com/yhirose/cpp-httplib)
- [spdlog 文档](https://github.com/gabime/spdlog)
- [nlohmann/json 文档](https://json.nlohmann.me/)

---

**报告结束**

*本报告基于 MCP Server 项目源代码分析生成，详细描述了项目的架构设计、核心实现和最佳实践。*
