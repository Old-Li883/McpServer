---
name: start-mcp-agent
description: 启动 MCP Agent 开发环境。自动检查并启动 Ollama、MCP Server，然后在新终端窗口启动 Python Agent。
---

# MCP Agent 启动流程

按以下顺序启动开发环境：

## 步骤 0: 检查并安装 Python 依赖

确保所需的 Python 包已安装：

```bash
# 检查并安装依赖
python3 -c "import yaml" 2>/dev/null || {
    echo "安装 Python 依赖..."
    pip3 install -e agent/ || pip3 install httpx pydantic pyyaml ollama rich prompt-toolkit loguru
    echo "✅ 依赖安装完成"
}
```

## 步骤 1: 检查并启动 Ollama

检查 Ollama 是否运行，如果未运行则启动：

```bash
# 检查进程
pgrep -x "ollama" > /dev/null && echo "Ollama 已运行" || {
    echo "启动 Ollama..."
    nohup ollama serve > /dev/null 2>&1 &
    sleep 3
}

# 验证服务可用
curl -s http://localhost:11434/api/tags > /dev/null && echo "✅ Ollama 服务正常" || echo "❌ Ollama 服务异常"
```

## 步骤 2: 检查并启动 MCP Server

检查 MCP Server 是否运行，如果未运行则启动：

```bash
# 检查进程
pgrep -f "McpServer" > /dev/null && echo "MCP Server 已运行" || {
    echo "启动 MCP Server..."
    mkdir -p logs
    nohup ./build/McpServer > logs/mcpserver_startup.log 2>&1 &
    sleep 3
}

# 验证服务可用
curl -s http://localhost:8080/health > /dev/null && echo "✅ MCP Server 服务正常" || echo "❌ MCP Server 服务异常"
```

## 步骤 3: 启动 Agent

在新终端窗口启动 Python Agent（使用模块方式）：

```bash
# macOS
osascript -e 'tell app "Terminal" to do script "cd '"$(pwd)"' && python3 -m agent.cli.main"'

# Linux
# gnome-terminal -- bash -c "cd $(pwd) && python3 -m agent.cli.main; exec bash"
```

## 成功条件

- Ollama API 可访问（端口 11434）
- MCP Server 健康检查通过（端口 8080）
- Agent CLI 在新终端窗口显示欢迎消息

## 故障排查

如果服务启动失败：
- 检查端口占用：`lsof -i :11434` 或 `lsof -i :8080`
- 查看 MCP Server 日志：`tail -f logs/mcpserver_startup.log`
- 重启服务：手动 kill 进程后重新执行此 skill