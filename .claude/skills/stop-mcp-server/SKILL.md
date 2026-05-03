---
name: stop-mcp-server
description: 停止正在运行的 MCP Server。安全终止 McpServer 进程并清理资源。当需要重启服务或释放端口时使用。
---

# 停止 MCP Server

安全停止正在运行的 MCP Server。

## 执行步骤

```bash
# 查找并停止 MCP Server 进程
if pgrep -f "McpServer" > /dev/null; then
    echo "停止 MCP Server..."
    pkill -f "McpServer"
    sleep 1

    # 验证进程已停止
    if pgrep -f "McpServer" > /dev/null; then
        echo "⚠️ 进程未响应，使用强制终止..."
        pkill -9 -f "McpServer"
    fi

    echo "✅ MCP Server 已停止"
else
    echo "ℹ️ MCP Server 未运行"
fi
```

## 验证停止

```bash
# 检查端口是否已释放
lsof -i :8080 && echo "❌ 端口 8080 仍被占用" || echo "✅ 端口 8080 已释放"

# 检查进程是否已清理
pgrep -f "McpServer" && echo "❌ 进程仍存在" || echo "✅ 进程已清理"
```

## 故障排查

如果服务无法停止：
- 使用 `ps aux | grep McpServer` 查找进程ID
- 手动终止：`kill -9 <PID>`
- 检查僵尸进程：`ps aux | grep defunct`