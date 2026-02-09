# JSON-RPC 模块设计文档

本目录包含 JSON-RPC 2.0 模块的 PlantUML 设计图。

## 文件说明

| 文件 | 描述 |
|------|------|
| `jsonrpc_sequence.puml` | 时序图 - 展示方法注册、请求处理、响应返回的完整流程 |
| `jsonrpc_usage.puml` | 使用流程图 - 展示如何初始化和使用 JSON-RPC 模块 |

## 如何查看

### 方式 1: 在线查看
访问 [PlantText](https://www.planttext.com/) 或 [PlantUML Online Editor](http://www.plantuml.com/plantuml/uml/)，将 `.puml` 文件内容粘贴进去即可渲染。

### 方式 2: VS Code 插件
1. 安装 [PlantUML](https://marketplace.visualstudio.com/items?itemName=jebbs.plantuml) 插件
2. 打开 `.puml` 文件
3. 按 `Alt+D` 预览

### 方式 3: 命令行生成
```bash
# 安装 PlantUML
brew install plantuml

# 生成 PNG 图片
plantuml docs/jsonrpc/jsonrpc_sequence.puml

# 生成 SVG 图片
plantuml -tsvg docs/jsonrpc/jsonrpc_sequence.puml
```

## 时序图关键流程

### 1. 方法注册
```
UserHandler → JsonRpc → MethodRegistry
注册方法名与处理器的映射关系
```

### 2. 请求处理
```
Client → StdioJsonRpc → JsonRpcSerialization → JsonRpc → MethodRegistry → UserHandler
```

### 3. 响应返回
```
UserHandler → JsonRpc → JsonRpcSerialization → StdioJsonRpc → Client
```

### 4. 通知处理（无 ID）
```
Client → StdioJsonRpc → JsonRpc → UserHandler
(不返回响应)
```

### 5. 错误处理
```
Handler 抛出异常 → JsonRpc 捕获 → JsonRpcSerialization 创建错误响应 → 返回给 Client
```

## 模块分层

```
┌─────────────────────────────────────────┐
│           上游: Client Process          │  stdin/stdout
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│    StdioJsonRpc (传输层)                │  I/O 处理
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│    JsonRpc (协议层)                      │  方法分发
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│    JsonRpcSerialization (序列化层)      │  数据转换
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│    UserHandler (业务逻辑)                │  用户实现
└─────────────────────────────────────────┘
```
