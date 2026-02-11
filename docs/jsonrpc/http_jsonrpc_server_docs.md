# HttpJsonRpcServer 架构文档

## 概述

`HttpJsonRpcServer` 是基于 `cpp-httplib` 实现的 HTTP JSON-RPC 2.0 服务器，支持同步/异步方法调用、SSE (Server-Sent Events) 实时推送、CORS 和自定义路由。

## 图表说明

### 1. 时序图 (http_jsonrpc_server_sequence.puml)

展示客户端与服务器之间的交互顺序，包括：

- **JSON-RPC 请求处理流程**
  - 客户端发送 HTTP POST 请求
  - 服务器解析并调用 JSON-RPC 处理器
  - 方法处理器执行业务逻辑
  - 返回 JSON 响应

- **SSE 实时推送流程**
  - 客户端建立 SSE 连接
  - 服务器生成客户端 ID 并确认连接
  - 业务逻辑触发事件推送
  - 服务器向客户端推送 SSE 事件

### 2. 流程图 (http_jsonrpc_server_flow.puml)

展示服务器处理不同类型请求的完整流程：

- **JSON-RPC 请求** (`/` 或 `/rpc`)
  - 请求解析 → 验证 → 方法分发 → 结果返回

- **SSE 连接** (自定义端点)
  - 连接建立 → 事件循环 → 心跳检测 → 连接清理

- **自定义路由**
  - GET/POST 自定义处理器
  - 静态文件服务
  - CORS Preflight 处理

### 3. 架构图 (http_jsonrpc_server_architecture.puml)

展示系统的整体架构和数据流向：

```
上游 (客户端) → HttpJsonRpcServer → JsonRpc (协议层) → 下游 (业务逻辑)
                      ↓
                 SSE Manager
                      ↓
              持久连接推送
```

#### 关键组件

| 组件 | 职责 |
|------|------|
| cpp-httplib | HTTP 服务器底座 |
| Request Router | 请求路由分发 |
| SSE Manager | SSE 连接和事件管理 |
| JsonRpc | JSON-RPC 2.0 协议处理 |
| Method Registry | 方法注册表 |

### 4. 状态图 (http_jsonrpc_server_state.puml)

展示服务器的生命周期状态转换：

```
Created → Starting → Running → Stopping → Stopped
                    ↓
                 Error
```

## 数据流说明

### JSON-RPC 请求流

```
输入 (上游):
{
  "jsonrpc": "2.0",
  "method": "add",
  "params": [1, 2],
  "id": 1
}

↓ HttpJsonRpcServer

↓ JsonRpc Protocol

↓ MethodHandler

输出 (下游):
{
  "jsonrpc": "2.0",
  "result": 3,
  "id": 1
}
```

### SSE 事件流

```
业务逻辑事件
    ↓
broadcastSseEvent(endpoint, event)
    ↓
SSE Manager
    ↓
event: update
data: {"value": 123}
    ↓
客户端接收
```

## 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| host | "0.0.0.0" | 监听地址 |
| port | 8080 | 监听端口 |
| threadCount | 4 | 工作线程数 |
| maxPayloadSize | 64MB | 最大请求体 |
| enableCors | false | 启用 CORS |
| enableSse | true | 启用 SSE |

## API 接口

### 服务器控制

```cpp
bool start();                    // 同步启动
bool startAsync();               // 异步启动
void stop();                     // 停止服务器
void wait();                     // 等待结束
bool isRunning();               // 检查状态
```

### SSE 管理

```cpp
bool registerSseEndpoint(endpoint, eventCallback, connectionCallback);
bool unregisterSseEndpoint(endpoint);
size_t broadcastSseEvent(endpoint, event);
bool sendSseEvent(endpoint, clientId, event);
size_t getSseClientCount(endpoint);
```

### 自定义路由

```cpp
bool registerGetHandler(path, handler);
bool registerPostHandler(path, handler);
bool setStaticFileDir(mountPoint, directory);
```

## 使用示例

```cpp
JsonRpc rpc;
HttpJsonRpcServerConfig config;
config.port = 8080;
config.enableCors = true;

HttpJsonRpcServer server(rpc, config);

// 注册 JSON-RPC 方法
rpc.registerMethod("add", [](auto&, auto& params) {
    return params[0].get<int>() + params[1].get<int>();
});

// 注册 SSE 端点
server.registerSseEndpoint("/events",
    [](auto endpoint, auto clientId, auto event) {
        // 事件回调
    },
    [](auto endpoint, auto clientId, bool connected) {
        // 连接状态回调
    }
);

server.startAsync();

// 广播事件
SseEvent event;
event.event = "update";
event.data = R"({"value": 123})";
server.broadcastSseEvent("/events", event);
```
