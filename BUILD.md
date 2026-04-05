# MCP Server 构建指南

## 快速开始

### 前提条件

确保已安装以下依赖（通过 Homebrew）：

```bash
# 安装必要的依赖库
brew install nlohmann-json spdlog fmt cpp-httplib openssl googletest
```

### 构建步骤

1. **创建构建目录并配置**
   ```bash
   mkdir build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   ```

2. **编译项目**
   ```bash
   make
   ```

   或者只编译主程序：
   ```bash
   make McpServer
   ```

3. **运行服务器**
   ```bash
   ./McpServer
   ```

   服务器将在 `http://0.0.0.0:8080` 上启动。

### 清理构建

```bash
# 清理构建文件
make clean

# 完全删除构建目录
cd ..
rm -rf build
```

## 运行测试

```bash
cd build
# 运行所有测试
make test

# 或者运行单个测试
./test_logger
./test_jsonrpc
./test_stdio_jsonrpc
./test_http_jsonrpc_server
./test_mcp_server
```

## MCP 协议测试

### 列出所有工具
```bash
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tools/list",
    "params": {}
  }'
```

### 调用工具
```bash
# 调用 add 工具
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/call",
    "params": {
      "name": "add",
      "arguments": {"a": 10, "b": 25}
    }
  }'

# 调用 echo 工具
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
      "name": "echo",
      "arguments": {"text": "Hello MCP!"}
    }
  }'
```

### 列出资源
```bash
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc": "2.0",
    "id": 4,
    "method": "resources/list"
  }'
```

## 构建选项

### Debug 模式
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build -S .
make -C build
```

### Release 模式
```bash
cmake -DCMAKE_BUILD_TYPE=Release -B build -S .
make -C build
```

## 故障排除

### 链接错误
如果遇到链接错误，确保所有依赖库都已正确安装：
```bash
brew reinstall nlohmann-json spdlog fmt cpp-httplib
```

### 端口占用
如果 8080 端口被占用，可以在 `config.json` 中修改端口配置。

## 配置文件

编辑 `config.json` 来自定义服务器行为：
- `serverMode`: "http", "stdio", 或 "both"
- `port`: 服务器端口（默认 8080）
- `logLevel`: 日志级别（debug, info, warning, error）
- `logFilePath`: 日志文件路径
