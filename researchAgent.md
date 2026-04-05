# MCP Agent 完整系统研究报告

**项目**: MCP Server + Python Agent
**语言**: C++17 + Python 3.10+
**协议**: MCP 2024-11-05 + JSON-RPC 2.0
**日期**: 2026-03-01

---

## 目录

1. [系统概述](#1-系统概述)
2. [架构设计](#2-架构设计)
3. [Python Agent 核心组件](#3-python-agent-核心组件)
4. [C++ MCP Server 核心组件](#4-c-mcp-server-核心组件)
5. [完整调用流程](#5-完整调用流程)
6. [工具调用机制](#6-工具调用机制)
7. [配置与扩展](#7-配置与扩展)
8. [关键发现与优化](#8-关键发现与优化)

---

## 1. 系统概述

### 1.1 项目定位

这是一个完整的 **MCP (Model Context Protocol) 客户端-服务端系统**：

- **C++ MCP Server**: 高性能的工具/资源/提示词服务器
- **Python Agent**: 使用本地 LLM (Ollama) 的智能代理客户端

### 1.2 技术架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                         用户交互层 (CLI)                             │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │  agent/cli/main.py - 交互式命令行界面                         │ │
│  │  - 命令解析  - 输入/输出  - 状态显示                          │ │
│  └───────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         Agent 编排层                               │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │  AgentEngine (agent/core/agent_engine.py)                     │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │ │
│  │  │ Conversation │  │ToolOrchestrator│ │PromptBuilder │        │ │
│  │  │   对话管理    │  │    工具编排    │  │   提示构建   │        │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘        │ │
│  │  ┌─────────────────────────────────────────────────────────┐  │ │
│  │  │    _process_with_tools() - ReAct 循环                  │  │ │
│  │  │    1. LLM 生成 → 2. 解析工具 → 3. 执行 → 4. 重新生成     │  │ │
│  │  └─────────────────────────────────────────────────────────┘  │ │
│  └───────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                    │                               │
                    ▼                               ▼
┌───────────────────────────────┐   ┌──────────────────────────────────┐
│      LLM 集成层               │   │      MCP 客户端层                │
│  ┌─────────────────────────┐  │   │  ┌────────────────────────────┐ │
│  │  OllamaClient           │  │   │  │  McpClient                │ │
│  │  - chat()               │  │   │  │  - initialize()           │ │
│  │  - generate()           │  │   │  │  - list_tools()            │ │
│  └─────────────────────────┘  │   │  │  - call_tool()             │ │
│  ┌─────────────────────────┐  │   │  └────────────────────────────┘ │
│  │  ResponseParser         │  │   └──────────────────────────────────┘
│  │  - 多格式工具调用解析   │  │                    │
│  └─────────────────────────┘  │                    ▼
└───────────────────────────────┘   ┌──────────────────────────────────┐
                                    │       C++ MCP Server              │
│  Python Process                │  ┌────────────────────────────────┐│
│  ┌──────────────────────────┐   │  │  McpServer                   ││
│  │ agent/                   │   │  │  - registerMethods()         ││
│  │ ├── cli/                 │   │  ├────────────────────────────────┤│
│  │ ├── core/                │   │  │  ToolManager                  ││
│  │ ├── llm/                 │   │  │  - registerTool()             ││
│  │ ├── mcp/                 │   │  │  - callTool()                 ││
│  │ └── config.py            │   │  ├────────────────────────────────┤│
│  │                          │   │  │  HttpJsonRpcServer           ││
│  │ 依赖:                     │   │  │  - HTTP POST handler         ││
│  │ - httpx (async HTTP)      │   │  │  - JSON-RPC 2.0              ││
│  │ - pydantic (validation)   │   │  └────────────────────────────────┘│
│  │ - rich (CLI美化)          │   │                                     │
│  │ - ollama (LLM)            │   │                                     │
│  └──────────────────────────┘   │                                     │
                                    │                                     │
└─────────────────────────────────┴─────────────────────────────────────┘
```

### 1.3 核心工作流程

```
用户输入 "What time is it now?"
        ↓
┌─────────────────────────────────────────────────────┐
│ 1. CLI 接收输入 → AgentEngine.process_message()    │
└─────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────┐
│ 2. 添加到对话 → _process_with_tools() 进入循环      │
└─────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────┐
│ 3. 第一次迭代:                                       │
│    - 构建 messages (系统提示 + 用户消息)             │
│    - OllamaClient.chat() 调用本地 LLM               │
│    - LLM 返回: "I don't have access to time..."    │
│    - ResponseParser 解析: 没有工具调用             │
│    - _infer_tool_calls() 推断: get_current_time    │
│    - ToolOrchestrator.execute_tool("get_current_time") │
└─────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────┐
│ 4. 工具执行:                                         │
│    - McpClient.call_tool() 发送 HTTP 请求           │
│    - POST http://localhost:8080/                    │
│    - C++ McpServer.handleToolsCall()                │
│    - ToolManager.callTool() 执行 lambda             │
│    - 返回: "Current time: Sun Mar 1 18:44:02 2026" │
└─────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────┐
│ 5. 第二次迭代:                                       │
│    - 对话历史包含工具结果                            │
│    - 再次调用 LLM                                   │
│    - LLM 返回空响应                                  │
│    - 检测到没有工具调用，退出循环                    │
│    - 返回工具结果给用户                              │
└─────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────┐
│ 6. CLI 显示结果给用户                               │
└─────────────────────────────────────────────────────┘
```

---

## 2. 架构设计

### 2.1 分层架构

```
┌───────────────────────────────────────────────────────────────┐
│                        表现层 (Presentation)                  │
│  - CLI 命令解析  - Rich 美化输出  - 用户交互                 │
├───────────────────────────────────────────────────────────────┤
│                        应用层 (Application)                   │
│  - AgentEngine 编排  - Conversation 管理  - 工具调用         │
├───────────────────────────────────────────────────────────────┤
│                        领域层 (Domain)                        │
│  - Message/Role  - Tool/ToolResult  - Prompt/Resource        │
├───────────────────────────────────────────────────────────────┤
│                        服务层 (Service)                       │
│  - OllamaClient (LLM)  - McpClient (MCP)  - ResponseParser   │
├───────────────────────────────────────────────────────────────┤
│                        基础设施层 (Infrastructure)            │
│  - HTTP (httpx)  - 配置 (Pydantic)  - 日志 (loguru)          │
└───────────────────────────────────────────────────────────────┘
```

### 2.2 数据流图

```
用户输入
    │
    ▼
┌──────────────┐
│ CLI Layer    │ 接收命令，判断是否为特殊命令
│  commands.py │
└──────┬───────┘
       │ 普通消息
       ▼
┌──────────────┐
│ AgentEngine  │ 核心编排，管理对话和工具
│process_msg() │
└──────┬───────┘
       │
       ▼
┌──────────────────────────────────────────────┐
│     _process_with_tools() 循环              │
│  ┌────────────────────────────────────────┐ │
│  │  messages = conversation.get_messages() │ │
│  └──────────────┬───────────────────────────┘ │
│                 ▼                            │
│  ┌────────────────────────────────────────┐ │
│  │  llm_response = llm_client.chat(msgs)  │ │
│  └──────────────┬───────────────────────────┘ │
│                 ▼                            │
│  ┌────────────────────────────────────────┐ │
│  │  parsed = response_parser.parse(response)│ │
│  │  inferred = infer_tools_if_needed()    │ │
│  └──────────────┬───────────────────────────┘ │
│                 ▼                            │
│  ┌────────────────────────────────────────┐ │
│  │  for tool_call in parsed.tool_calls:   │ │
│  │    result = tool_orchestrator.execute() │ │
│  └──────────────┬───────────────────────────┘ │
└─────────────────┼──────────────────────────────┘
                  │
                  ▼
            ┌──────────────┐
            │     输出      │
            └──────────────┘
```

---

## 3. Python Agent 核心组件

### 3.1 AgentEngine - 编排引擎

**文件**: `agent/core/agent_engine.py`

AgentEngine 是整个系统的核心，实现了 ReAct (Reasoning + Acting) 模式的工具调用编排。

#### 类结构

```python
class AgentEngine:
    def __init__(self, config, mcp_client, llm_client):
        self.config = config
        self.conversation = Conversation(max_history=100)
        self.tool_orchestrator = ToolOrchestrator(mcp_client, response_parser)
        self.prompt_builder = PromptBuilder(system_prompt)
        self.llm_client = llm_client
```

#### 核心方法详解

##### process_message()

```python
async def process_message(self, user_message: str, max_tool_iterations: int = 5) -> str:
    """
    消息处理入口点

    Args:
        user_message: 用户输入的消息
        max_tool_iterations: 最大工具调用迭代次数

    Returns:
        Agent 的最终响应
    """
    # 1. 记录用户消息
    self.conversation.add_user_message(user_message)

    # 2. 进入工具调用循环
    response = await self._process_with_tools(max_tool_iterations, user_message)

    return response
```

##### _process_with_tools() - 核心编排循环

这是系统最复杂的方法，实现了完整的 ReAct 循环：

```python
async def _process_with_tools(self, max_iterations: int, original_user_message: str = ""):
    """
    工具调用编排循环 - ReAct 模式实现

    实现步骤:
    1. 获取对话历史并构建 LLM 请求
    2. 调用 LLM 生成响应
    3. 解析响应中的工具调用
    4. 如果没有工具调用，尝试智能推断
    5. 执行所有工具调用
    6. 将工具结果添加到对话
    7. 如果有工具调用，继续下一轮迭代
    8. 否则返回最终响应
    """
    iteration = 0
    final_response = ""
    used_tools = set()  # 防止重复调用同一工具

    while iteration < max_iterations:
        iteration += 1

        # === 步骤 1: 获取对话上下文 ===
        messages = self.conversation.get_messages_for_llm()
        # messages 格式: [
        #   {"role": "system", "content": "You are a helpful AI... ## Available Tools..."},
        #   {"role": "user", "content": "What time is it now?"},
        #   {"role": "assistant", "content": "...", "tool_calls": [...]},
        #   {"role": "tool", "content": "[Tool: xxx] ..."}
        # ]

        # === 步骤 2: LLM 生成响应 ===
        llm_response = await self._generate_response(messages)
        # 内部调用: OllamaClient.chat(messages, temperature, max_tokens)
        # 发送到: http://localhost:11434/api/chat

        # === 步骤 3: 解析工具调用 ===
        available_tool_names = self.tool_orchestrator.get_tool_names()
        parsed = self.tool_orchestrator.parse_tool_calls(llm_response, available_tool_names)

        # === 步骤 4: 智能推断 (仅第一次迭代) ===
        if not parsed.has_tool_calls and iteration == 1 and original_user_message:
            inferred_calls = self._infer_tool_calls(original_user_message, available_tool_names)
            if inferred_calls:
                # 过滤掉已使用的工具
                new_calls = [c for c in inferred_calls if c.name not in used_tools]
                if new_calls:
                    parsed.tool_calls.extend(new_calls)

        # === 步骤 5: 记录助手响应 ===
        self.conversation.add_assistant_message(
            parsed.text,
            tool_calls=[tc.to_dict() for tc in parsed.tool_calls] if parsed.tool_calls else None,
        )

        # === 步骤 6: 检查是否需要退出 ===
        if not parsed.tool_calls:
            # 没有工具调用，生成最终响应
            if not parsed.text.strip() and used_tools:
                # LLM 返回空文本，使用工具结果
                for msg in reversed(self.conversation.get_messages()):
                    if msg.role == Role.TOOL:
                        final_response = msg.content
                        # 清理工具结果格式
                        if '[' in final_response and ']' in final_response:
                            final_response = final_response.split(']', 1)[-1].strip()
                        break
            else:
                final_response = parsed.text if parsed.text.strip() else "无法生成响应"
            break

        # === 步骤 7: 执行工具调用 ===
        for tool_call in parsed.tool_calls:
            # 跳过已执行的工具
            if tool_call.name in used_tools:
                continue

            used_tools.add(tool_call.name)
            try:
                # 调用 ToolOrchestrator 执行工具
                result = await self.tool_orchestrator.execute_tool(
                    tool_call.name,
                    tool_call.arguments,
                )

                # 格式化工具结果
                result_text = self.tool_orchestrator.format_tool_result(
                    tool_call.name,
                    result,
                )

                # 添加工具结果到对话
                self.conversation.add_tool_message(tool_call.name, result_text)

            except Exception as e:
                # 工具执行错误
                self.conversation.add_tool_message(
                    tool_call.name,
                    f"Error: {str(e)}",
                )

    # === 步骤 8: 达到最大迭代次数 ===
    if iteration >= max_iterations:
        final_response = "达到最大工具调用次数，无法完成请求。"

    return final_response
```

##### _infer_tool_calls() - 智能工具推断

```python
def _infer_tool_calls(self, user_message: str, available_tools: list[str]):
    """
    当 LLM 没有按格式输出工具调用时，自动推断

    使用模式匹配技术识别用户意图：
    - 时间查询 → get_current_time
    - 回显请求 → echo
    - 计算请求 → add
    """
    import re
    from agent.llm.response_parser import ToolCall

    calls = []
    message_lower = user_message.lower()

    # 工具意图模式映射
    tool_patterns = {
        'get_current_time': [
            r'\b(time|current time|date|what time|what\'s the time)\b',
            r'\b(now|today|clock)\b'
        ],
        'echo': [
            r'\b(echo|repeat|say again|tell me again)\b',
            r'\b(say\s+[\'"].*?[\'"])'
        ],
        'add': [
            r'\b(add|plus|sum|calculate|total)\b.*\d+.*\d+',
            r'\d+\s*[\+]\s*\d+',
        ]
    }

    for tool_name, patterns in tool_patterns.items():
        if tool_name not in available_tools:
            continue

        for pattern in patterns:
            if re.search(pattern, message_lower, re.IGNORECASE):
                # 提取参数
                arguments = self._extract_arguments(tool_name, user_message)
                calls.append(ToolCall(name=tool_name, arguments=arguments))
                break

    return calls

def _extract_arguments(self, tool_name: str, user_message: str):
    """从用户消息中提取工具参数"""
    import re

    if tool_name == 'echo':
        # 提取引号中的文本
        match = re.search(r'(?:echo|repeat|say)\s+[\'"](.+?)[\'"]', user_message, re.IGNORECASE)
        if match:
            return {'text': match.group(1)}
        # 使用剩余文本
        text = re.sub(r'^(echo|repeat|say)\s*', '', user_message, flags=re.IGNORECASE)
        return {'text': text.strip()}

    elif tool_name == 'add':
        # 提取两个数字
        numbers = re.findall(r'\d+\.?\d*', user_message)
        if len(numbers) >= 2:
            return {'a': float(numbers[0]), 'b': float(numbers[1])}

    return {}
```

### 3.2 Conversation - 对话管理

**文件**: `agent/core/conversation.py`

#### 数据结构

```python
@dataclass
class Message:
    role: Role           # SYSTEM, USER, ASSISTANT, TOOL
    content: str
    timestamp: datetime
    tool_calls: Optional[list[dict]]  # ASSISTANT 消息的工具调用
    tool_results: Optional[dict]      # TOOL 消息的结果

class Role(str, Enum):
    SYSTEM = "system"
    USER = "user"
    ASSISTANT = "assistant"
    TOOL = "tool"
```

#### 对话管理功能

```python
class Conversation:
    def __init__(self, max_history: int = 100):
        self.max_history = max_history
        self.messages: list[Message] = []
        self._system_prompt: Optional[str] = None

    def add_user_message(self, content: str) -> Message:
        """添加用户消息"""
        msg = Message.user(content)
        self.add_message(msg)
        return msg

    def add_assistant_message(self, content: str, tool_calls = None) -> Message:
        """添加助手消息（可包含工具调用）"""
        msg = Message.assistant(content, tool_calls)
        self.add_message(msg)
        return msg

    def add_tool_message(self, tool_name: str, content: str) -> Message:
        """添加工具结果消息"""
        msg = Message.tool(tool_name, content)
        self.add_message(msg)
        return msg

    def get_messages_for_llm(self) -> list[dict]:
        """转换为 LLM API 格式

        Returns:
            [
                {"role": "system", "content": "..."},
                {"role": "user", "content": "What time is it now?"},
                {"role": "assistant", "content": "...", "tool_calls": [...]},
                {"role": "tool", "content": "[Tool: xxx] ..."}
            ]
        """
        messages = self.get_messages(include_system=True)

        # 如果没有系统提示，添加默认的
        if self._system_prompt and not any(m.role == Role.SYSTEM for m in messages):
            messages = [Message.system(self._system_prompt)] + messages

        return [m.to_dict() for m in messages]
```

### 3.3 ToolOrchestrator - 工具编排

**文件**: `agent/core/tools.py`

```python
class ToolOrchestrator:
    """工具发现、验证和执行的编排器"""

    def __init__(self, mcp_client, response_parser=None):
        self.mcp_client = mcp_client
        self.response_parser = response_parser or ResponseParser()
        self._available_tools: list[Tool] = []

    async def load_tools(self) -> list[Tool]:
        """从 MCP Server 加载可用工具"""
        self._available_tools = await self.mcp_client.list_tools()
        return self._available_tools

    async def execute_tool(self, name: str, arguments: dict) -> ToolResult:
        """执行工具的完整流程

        流程:
        1. 验证工具存在
        2. 验证参数符合 schema
        3. 调用 MCP 客户端
        4. 返回结果
        """
        # 1. 验证工具存在
        tool = self.get_tool(name)
        if tool is None:
            raise ToolExecutionError(f"Tool '{name}' not found")

        # 2. 验证参数
        self._validate_arguments(tool, arguments)

        # 3. 执行工具
        result = await self.mcp_client.call_tool(name, arguments)
        return result

    def _validate_arguments(self, tool: Tool, arguments: dict):
        """验证参数符合工具的 JSON Schema"""
        schema = tool.input_schema

        # 检查必需参数
        for required_param in schema.required:
            if required_param not in arguments:
                raise ToolExecutionError(
                    f"Missing required parameter '{required_param}' for tool '{tool.name}'"
                )
```

### 3.4 McpClient - MCP 协议客户端

**文件**: `agent/mcp/client.py`

```python
class McpClient:
    """MCP 协议的 HTTP 客户端实现"""

    async def connect(self):
        """连接并初始化与 MCP Server 的会话

        流程:
        1. 检查服务器健康状态
        2. 调用 initialize 方法
        3. 保存服务器能力
        """
        # 创建 HTTP 客户端
        self._client = httpx.AsyncClient(timeout=self.timeout)

        # 健康检查
        response = await self._client.get(f"{self.server_url}/health")
        response.raise_for_status()

        # 初始化握手
        await self.initialize()

    async def initialize(self) -> InitializeResult:
        """MCP 协议初始化握手

        POST {server_url}/
        {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "mcp-agent", "version": "0.1.0"}
            }
        }
        """
        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "mcp-agent", "version": "0.1.0"}
            }
        }

        response = await self._client.post(
            f"{self.server_url}/",
            json=request,
            headers={"Content-Type": "application/json"}
        )

        data = response.json()
        return InitializeResult.from_dict(data.get("result", {}))

    async def list_tools(self) -> list[Tool]:
        """列出 MCP Server 提供的所有工具"""
        request = {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/list"
        }

        response = await self._client.post(f"{self.server_url}/", json=request)
        data = response.json()

        tools_data = data.get("result", {}).get("tools", [])
        return [Tool.from_dict(t) for t in tools_data]

    async def call_tool(self, name: str, arguments: dict) -> ToolResult:
        """调用指定工具

        POST {server_url}/
        {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/call",
            "params": {
                "name": "get_current_time",
                "arguments": {}
            }
        }
        """
        request = {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/call",
            "params": {
                "name": name,
                "arguments": arguments
            }
        }

        response = await self._client.post(f"{self.server_url}/", json=request)
        data = response.json()

        return ToolResult.from_dict(data.get("result", {}))
```

### 3.5 ResponseParser - 工具调用解析

**文件**: `agent/llm/response_parser.py`

支持 6 种工具调用格式：

```python
class ResponseParser:
    # 格式 1: 标准块格式
    TOOL_CALL_PATTERN = r"TOOL:\s*(\w+)\s*\nARGUMENTS:\s*(\{.*?\})"

    # 格式 2: 内联代码块
    INLINE_TOOL_PATTERN = r"```?\s*TOOL:\s*(\w+)\s+ARGUMENTS:\s*(\{.*?\})\s*```?"

    # 格式 3: 自然语言命令
    NATURAL_TOOL_PATTERN = r'(?:use|call|invoke)\s+(?:the\s+)?["\']?(\w+)["\']?'

    # 格式 4: 动作描述
    ACTION_PATTERN = r'(?:I\s+will|let\s+me)\s+(?:get|fetch|calculate)\s+(.+)'

    # 格式 5: 函数调用风格
    FUNCTION_STYLE_PATTERN = r'(\w+)\s*\(\s*(\{.*?\}.*?)?\s*\)'

    # 格式 6: OpenAI 函数调用
    OPENAI_FUNCTION_PATTERN = r'"name"\s*:\s*"(\w+)"'

    def parse(self, response: str, available_tools: list[str] = None):
        """解析 LLM 响应，提取所有工具调用"""
        tool_calls = []

        # 按优先级尝试各种格式
        tool_calls.extend(self._parse_tool_blocks(response))
        tool_calls.extend(self._parse_inline_tools(response))
        tool_calls.extend(self._parse_function_calls(response))
        tool_calls.extend(self._parse_natural_tools(response, available_tools or []))
        tool_calls.extend(self._parse_action_tools(response, available_tools or []))
        tool_calls.extend(self._parse_function_style(response))

        return ParsedResponse(text=response, tool_calls=tool_calls)
```

### 3.6 OllamaClient - LLM 客户端

**文件**: `agent/llm/ollama_client.py`

```python
class OllamaClient:
    """Ollama 本地 LLM 的客户端"""

    async def chat(self, messages: list[dict], temperature: float = 0.7,
                   max_tokens: int = 2048) -> str:
        """发送聊天请求到 Ollama

        POST http://localhost:11434/api/chat
        {
            "model": "qwen2:0.5b",
            "messages": [
                {"role": "system", "content": "You are..."},
                {"role": "user", "content": "What time is it now?"}
            ],
            "stream": false,
            "options": {
                "temperature": 0.7,
                "num_predict": 2048
            }
        }
        """
        request_body = {
            "model": self.model,
            "messages": messages,
            "stream": False,
            "options": {
                "temperature": temperature,
                "num_predict": max_tokens,
            },
        }

        response = await self._client.post(
            f"{self.base_url}/api/chat",
            json=request_body,
            timeout=self.timeout
        )

        data = response.json()
        return data.get("message", {}).get("content", "")
```

### 3.7 CLI - 命令行界面

**文件**: `agent/cli/main.py`

```python
async def interactive_loop(agent: AgentEngine, config: Config):
    """主交互循环"""
    display = Display()
    commands = CLICommands(agent, display)

    display.print_welcome(__version__)

    while True:
        # 获取用户输入
        prompt = commands.get_prompt()
        user_input = display.input(prompt)

        # 检查特殊命令
        if commands.is_command(user_input):
            should_continue = await commands.execute(user_input)
            if not should_continue:
                break
            continue

        # 处理普通消息
        display.print_thinking()

        try:
            response = await agent.process_message(user_input)
            display.print_message(response, role=MessageRole.ASSISTANT)
        except Exception as e:
            display.print_error(f"处理消息失败: {e}")
```

---

## 4. C++ MCP Server 核心组件

### 4.1 McpServer - MCP 协议服务器

**文件**: `src/mcp/McpServer.cpp`

```cpp
class McpServer {
private:
    std::unique_ptr<ToolManager> tool_manager_;
    std::unique_ptr<ResourceManager> resource_manager_;
    std::unique_ptr<PromptManager> prompt_manager_;

public:
    // 注册 MCP 方法到 JSON-RPC
    void registerMethods(json_rpc::JsonRpc& jsonRpc) {
        // 注册 initialize
        jsonRpc.registerMethod("initialize",
            [this](const std::string& method, const nlohmann::json& params) {
                return this->handleInitialize(params);
            }
        );

        // 注册 tools/list
        jsonRpc.registerMethod("tools/list",
            [this](const std::string& method, const nlohmann::json& params) {
                return this->handleToolsList(params);
            }
        );

        // 注册 tools/call
        jsonRpc.registerMethod("tools/call",
            [this](const std::string& method, const nlohmann::json& params) {
                return this->handleToolsCall(params);
            }
        );

        // ... resources/list, resources/read, prompts/list, prompts/get
    }

    // 工具调用处理器
    nlohmann::json handleToolsCall(const nlohmann::json& params) {
        LOG_DEBUG("Handling tools/call request");

        // 提取参数
        std::string name = params["name"].get<std::string>();
        nlohmann::json arguments = params.value("arguments", nlohmann::json::object());

        // 调用工具管理器
        auto result = tool_manager_->callTool(name, arguments);
        return result.to_json();
    }
};
```

### 4.2 ToolManager - 工具管理器

**文件**: `src/mcp/ToolManager.cpp`

```cpp
class ToolManager {
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ToolEntry> tools_;

public:
    bool registerTool(std::string name, std::string description,
                      ToolInputSchema input_schema, ToolHandler handler) {
        Tool tool;
        tool.name = std::move(name);
        tool.description = std::move(description);
        tool.input_schema = std::move(input_schema);

        return registerTool(std::move(tool), std::move(handler));
    }

    ToolResult callTool(const std::string& name, const nlohmann::json& arguments) {
        // 1. 先查找工具（持有锁）
        ToolHandler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = tools_.find(name);
            if (it == tools_.end()) {
                return ToolResult::error("Tool not found: " + name);
            }
            handler = it->second.handler;
        }  // 释放锁

        // 2. 调用处理器（无锁，避免死锁）
        try {
            LOG_DEBUG("Calling tool: {}", name);
            return handler(name, arguments);
        } catch (const std::exception& e) {
            LOG_ERROR("Error calling tool {}: {}", name, e.what());
            return ToolResult::error("Exception: "s + e.what());
        }
    }
};
```

### 4.3 HttpJsonRpcServer - HTTP 服务器

**文件**: `src/json_rpc/http_jsonrpc_server.cpp`

```cpp
class HttpJsonRpcServer {
public:
    bool start() {
        // 设置 HTTP 路由
        server_.Post("/", [this](const Request& req, Response& res) {
            // 处理 JSON-RPC 请求
            std::string response_str = handleJsonRpcRequest(req.body);
            res.set_content(response_str, "application/json");
        });

        // 健康检查端点
        server_.Get("/health", [](const Request& req, Response& res) {
            nlohmann::json health = {{"status", "ok"}};
            res.set_content(health.dump(), "application/json");
        });

        // 启动服务器
        return server_.listen(host_, port_);
    }

private:
    std::string handleJsonRpcRequest(const std::string& body) {
        // 解析 JSON-RPC 请求
        auto json_req = nlohmann::json::parse(body);
        std::string method = json_req["method"];
        nlohmann::json params = json_req["params"];

        // 调用对应的处理器
        auto handler = method_handlers_[method];
        nlohmann::json result = handler(method, params);

        // 构造 JSON-RPC 响应
        nlohmann::json response = {
            {"jsonrpc", "2.0"},
            {"id", json_req["id"]},
            {"result", result}
        };

        return response.dump();
    }
};
```

---

## 5. 完整调用流程

### 5.1 用户输入 "What time is it now?" 的详细流程

#### 完整时序图

```
用户     CLI    AgentEngine    OllamaClient    Ollama     McpClient    McpServer
 │        │         │              │           │           │            │
 │ "What │         │              │           │           │            │
 │ time?"│         │              │           │           │            │
 ├───────>│         │              │           │           │            │
 │        │ process │              │           │           │            │
 │        │  _message()           │           │           │            │
 │        ├────────>│              │           │           │            │
 │        │         │ chat()       │           │           │            │
 │        │         ├─────────────>│           │           │            │
 │        │         │              │ POST /api/chat           │            │
 │        │         │              ├───────────>│           │            │
 │        │         │              │           │  推理生成              │            │
 │        │         │              │           │  "I don't..."       │            │
 │        │         │              │<───────────┤           │            │
 │        │         │<─────────────┤           │           │            │
 │        │         │  parse()      │           │           │            │
 │        │         │  (无工具调用)  │           │           │            │
 │        │         │              │           │           │            │
 │        │         │  infer_tools()            │           │            │
 │        │         │  → get_current_time       │           │            │
 │        │         │              │           │           │            │
 │        │         │ execute_tool()           │           │            │
 │        │         ├───────────────────────────>│           │            │
 │        │         │              │           │  POST /    │            │
 │        │         │              │           │  tools/call│            │
 │        │         │              │           ├──────────>│            │
 │        │         │              │           │           │  handleToolsCall()
 │        │         │              │           │           │  ├───────────>│
 │        │         │              │           │           │  │  callTool()│
 │        │         │              │           │           │  │  执行 lambda│
 │        │         │              │           │           │  │  返回时间  │
 │        │         │              │           │           │<────────────┤
 │        │         │              │           │           │<───────────┤
 │        │         │              │           │  ToolResult │            │
 │        │         │              │           │<───────────┤           │            │
 │        │         │<───────────────────────────┤           │            │
 │        │         │              │           │           │            │
 │        │         │  chat() with tool result    │           │            │
 │        │         ├─────────────>│           │           │            │
 │        │         │              ├───────────>│           │            │
 │        │         │              │  "" (空)    │           │            │
 │        │         │              │<───────────┤           │            │
 │        │         │<─────────────┤           │           │            │
 │        │         │  提取工具结果  │           │           │            │
 │        │<────────┤              │           │           │            │
 │        │  "Current time: ..."           │           │            │            │
 │        │         │              │           │           │            │
```

### 5.2 代码级调用链

```python
# ===== 步骤 1: CLI 接收输入 =====
# agent/cli/main.py:42
user_input = display.input("> ")  # "What time is it now?"

# ===== 步骤 2: Agent 处理 =====
# agent/cli/main.py:69
response = await agent.process_message(user_input)

# agent/core/agent_engine.py:66
self.conversation.add_user_message(user_message)
response = await self._process_with_tools(max_tool_iterations, user_input)

# ===== 步骤 3: 第一次迭代开始 =====
# agent/core/agent_engine.py:110
iteration = 1

# agent/core/agent_engine.py:114
messages = self.conversation.get_messages_for_llm()
# [
#   {"role": "system", "content": "You are a helpful AI... ## Available Tools..."},
#   {"role": "user", "content": "What time is it now?"}
# ]

# agent/core/agent_engine.py:117
llm_response = await self._generate_response(messages)

# ===== 步骤 4: 调用 Ollama =====
# agent/core/agent_engine.py:166
response = await self.llm_client.chat(
    messages=messages,
    temperature=self.config.llm.temperature,
    max_tokens=self.config.llm.max_tokens,
)

# agent/llm/ollama_client.py:173
request_body = {
    "model": "qwen2:0.5b",
    "messages": messages,
    "stream": False,
    "options": {"temperature": 0.7, "num_predict": 2048},
}

# agent/llm/ollama_client.py:172
response = await self._client.post(
    f"{self.base_url}/api/chat",
    json=request_body,
    timeout=self.timeout,
)

# Ollama 返回
llm_response = "I don't have access to current time in real-time..."

# ===== 步骤 5: 解析工具调用 =====
# agent/core/agent_engine.py:118
available_tool_names = self.tool_orchestrator.get_tool_names()
# ["get_current_time", "add", "echo"]

# agent/core/agent_engine.py:119
parsed = self.tool_orchestrator.parse_tool_calls(llm_response, available_tool_names)

# agent/core/tools.py:173
return self.response_parser.parse(response, available_tools or [])

# agent/llm/response_parser.py:61
# 尝试各种格式，都没有匹配
parsed.has_tool_calls = False

# ===== 步骤 6: 智能推断 =====
# agent/core/agent_engine.py:121-124
if not parsed.has_tool_calls and iteration == 1:
    inferred_calls = self._infer_tool_calls(original_user_message, available_tool_names)

# agent/core/agent_engine.py:_infer_tool_calls()
# 匹配 r'\btime\b' → True
# 返回 [ToolCall(name="get_current_time", arguments={})]

parsed.tool_calls.extend(inferred_calls)

# ===== 步骤 7: 记录助手响应 =====
# agent/core/agent_engine.py:127-130
self.conversation.add_assistant_message(
    parsed.text,
    tool_calls=[tc.to_dict() for tc in parsed.tool_calls],
)

# ===== 步骤 8: 执行工具 =====
# agent/core/agent_engine.py:138-143
for tool_call in parsed.tool_calls:
    result = await self.tool_orchestrator.execute_tool(
        tool_call.name,
        tool_call.arguments,
    )

# agent/core/tools.py:113-140
async def execute_tool(self, name: str, arguments: dict):
    # 验证工具存在
    tool = self.get_tool(name)

    # 验证参数
    self._validate_arguments(tool, arguments)

    # 调用 MCP 客户端
    result = await self.mcp_client.call_tool(name, arguments)
    return result

# agent/mcp/client.py:208-230
async def call_tool(self, name: str, arguments: dict):
    request = {
        "jsonrpc": "2.0",
        "id": 3,
        "method": "tools/call",
        "params": {
            "name": name,
            "arguments": arguments
        }
    }

    response = await self._client.post(
        f"{self.server_url}/",
        json=request,
    )

    return ToolResult.from_dict(response.json().get("result", {}))

# ===== 步骤 9: C++ 处理 =====
# src/json_rpc/http_jsonrpc_server.cpp
server_.Post("/", ...)

# src/mcp/McpServer.cpp:231
nlohmann::json McpServer::handleToolsCall(const nlohmann::json& params) {
    std::string name = params["name"];  // "get_current_time"
    nlohmann::json arguments = params.value("arguments", json::object());

    auto result = tool_manager_->callTool(name, arguments);
    return result.to_json();
}

# src/mcp/ToolManager.cpp:88
ToolResult ToolManager::callTool(const std::string& name, const nlohmann::json& arguments) {
    // 查找工具处理器
    auto it = tools_.find("get_current_time");
    ToolHandler handler = it->second.handler;

    // 执行处理器
    LOG_DEBUG("Calling tool: {}", name);
    return handler(name, arguments);
}

# src/main.cpp:62-82 (注册时的 lambda)
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

# ===== 步骤 10: 返回结果 =====
# C++ 返回 JSON
{
    "content": [{"type": "text", "text": "Current time: Sun Mar  1 18:44:02 2026"}]
}

# Python 接收
# agent/mcp/types.py:128
items = [ContentItem.from_dict(item) for item in data["content"]]
return cls(content=items, is_error=data.get("isError", False))

# ===== 步骤 11: 添加工具结果到对话 =====
# agent/core/agent_engine.py:146-150
result_text = self.tool_orchestrator.format_tool_result(
    tool_call.name,
    result,
)
self.conversation.add_tool_message(tool_call.name, result_text)

# agent/core/tools.py:213-233
def format_tool_result(self, tool_name: str, result: ToolResult):
    parts = []
    for item in result.content:
        if item.type == "text" and item.text:
            parts.append(item.text)
    return "\n".join(parts)

# agent/core/conversation.py:137-149
def add_tool_message(self, tool_name: str, content: str):
    msg = Message.tool(tool_name, content)
    self.add_message(msg)

# ===== 步骤 12: 第二次迭代 =====
# 对话历史现在有 4 条消息
iteration = 2
messages = [
    {"role": "system", "content": "..."},
    {"role": "user", "content": "What time is it now?"},
    {"role": "assistant", "content": "I don't have...", "tool_calls": [...]},
    {"role": "tool", "content": "[Tool: get_current_time] Current time: ..."}
]

# 再次调用 LLM
llm_response = await self._generate_response(messages)
# 返回: "" (空字符串)

# ===== 步骤 13: 退出循环并返回结果 =====
# agent/core/agent_engine.py:133-135
if not parsed.tool_calls:
    # 检查是否有工具结果
    if not parsed.text.strip() and used_tools:
        for msg in reversed(self.conversation.get_messages()):
            if msg.role == Role.TOOL:
                final_response = msg.content
                # 清理格式
                if '[' in final_response and ']' in final_response:
                    final_response = final_response.split(']', 1)[-1].strip()
                break

# ===== 步骤 14: CLI 显示 =====
# agent/cli/main.py:72
display.print_message(response, role=MessageRole.ASSISTANT)

# agent/cli/display.py:46-78
def print_message(self, content: str, role: MessageRole):
    if role == MessageRole.ASSISTANT:
        md = Markdown(content)  # 使用 Rich 渲染 Markdown
        self.console.print(md)

# 用户看到: Current time: Sun Mar  1 18:44:02 2026
```

---

## 6. 工具调用机制

### 6.1 工具注册 (C++ 端)

**文件**: `src/main.cpp`

```cpp
void registerExampleTools(McpServerRunner& runner) {
    // 工具 1: get_current_time
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

    // 工具 2: echo
    runner.registerTool(
        "echo",
        "Echo back the input text",
        ToolInputSchema{
            {"type", "object"},
            {{
                {"properties", {{
                    {"text", {
                        {"type", "string"},
                        {"description", "The text to echo back"}
                    }}
                }},
                {"required", {"text"}}
            }}
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            std::string text = args.value("text", "");
            return ToolResult::success({
                ContentItem::text_content("Echo: " + text)
            });
        }
    );

    // 工具 3: add
    runner.registerTool(
        "add",
        "Add two numbers together",
        ToolInputSchema{
            {"type", "object"},
            {{
                {"properties", {{
                    {"a", {{"type", "number"}, {"description", "First number"}}},
                    {"b", {{"type", "number"}, {"description", "Second number"}}}
                }},
                {"required", {"a", "b"}}
            }}
        },
        [](const std::string& name, const nlohmann::json& args) -> ToolResult {
            double a = args.value("a", 0.0);
            double b = args.value("b", 0.0);
            double result = a + b;
            return ToolResult::success({
                ContentItem::text_content(
                    std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(result)
                )
            });
        }
    );
}
```

### 6.2 工具调用完整链路

```
用户输入 "Add 15 and 27"
        ↓
┌─────────────────────────────────────────────────────────┐
│ Python Agent 端                                           │
├─────────────────────────────────────────────────────────┤
│ 1. AgentEngine 收到输入                                  │
│ 2. _infer_tool_calls() 匹配到 "add" 模式                │
│    - 提取参数: a=15, b=27                                │
│ 3. ToolOrchestrator.execute_tool("add", {"a":15, "b":27}) │
│ 4. McpClient.call_tool() 发送 HTTP 请求                  │
└─────────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────────┐
│ HTTP 传输层                                               │
├─────────────────────────────────────────────────────────┤
│ POST http://localhost:8080/                              │
│ Content-Type: application/json                          │
│ {                                                         │
│   "jsonrpc": "2.0",                                      │
│   "method": "tools/call",                                 │
│   "params": {                                            │
│     "name": "add",                                       │
│     "arguments": {"a": 15, "b": 27}                      │
│   }                                                       │
│ }                                                         │
└─────────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────────┐
│ C++ Server 端                                             │
├─────────────────────────────────────────────────────────┤
│ 1. HttpJsonRpcServer 接收请求                             │
│ 2. JsonRpc 路由到 "tools/call" 处理器                    │
│ 3. McpServer::handleToolsCall()                          │
│ 4. ToolManager::callTool("add", {"a":15, "b":27})       │
│ 5. 执行注册的 lambda 函数                                 │
│    - a = 15.0, b = 27.0                                  │
│    - result = 42.0                                       │
│    - 返回 ToolResult("15.000000 + 27.000000 = 42.000000") │
└─────────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────────┐
│ HTTP 响应                                                 │
├─────────────────────────────────────────────────────────┤
│ {                                                         │
│   "jsonrpc": "2.0",                                      │
│   "result": {                                            │
│     "content": [                                          │
│       {"type": "text", "text": "15.000000 + 27.000000 = 42.000000"} │
│     ]                                                     │
│   }                                                       │
│ }                                                         │
└─────────────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────────────┐
│ Python Agent 端 (接收)                                   │
├─────────────────────────────────────────────────────────┤
│ 1. ToolResult.from_dict() 解析响应                       │
│ 2. Conversation.add_tool_message("add", result_text)    │
│ 3. 第二次迭代，LLM 基于工具结果生成最终响应              │
│ 4. 返回 "42" 给用户                                       │
└─────────────────────────────────────────────────────────┘
```

---

## 7. 配置与扩展

### 7.1 配置文件

**文件**: `agent/config.yaml`

```yaml
# LLM 配置
llm:
  base_url: "http://localhost:11434"
  model: "qwen2:0.5b"  # 可选: qwen3-vl:8b, llama3.2
  temperature: 0.7
  max_tokens: 2048

# MCP Server 配置
mcp:
  server_url: "http://localhost:8080"
  timeout: 30

# Agent 配置
agent:
  name: "mcp-agent"
  log_level: "INFO"

# 对话配置
conversation:
  max_history: 100
  system_prompt: |
    You are a helpful AI assistant with access to tools and resources.

# CLI 配置
cli:
  prompt_symbol: "> "
  show_thinking: true
```

### 7.2 添加新工具

#### 步骤 1: 在 C++ 端注册

```cpp
// src/main.cpp
runner.registerTool(
    "calculate_weather",
    "Get weather information for a city",
    ToolInputSchema{
        {"type", "object"},
        {{
            {"properties", {{
                {"city", {
                    {"type", "string"},
                    {"description", "City name"}
                }}
            }},
            {"required", {"city"}}
        }}
    },
    [](const std::string& name, const nlohmann::json& args) -> ToolResult {
        std::string city = args.value("city", "");
        // 调用天气 API
        std::string weather = getWeatherFromAPI(city);
        return ToolResult::success({
            ContentItem::text_content("Weather in " + city + ": " + weather)
        });
    }
);
```

#### 步骤 2: 重新编译

```bash
cd build
make -j$(sysctl -n hw.ncpu)
./McpServer
```

#### 步骤 3: Python 自动发现

```bash
# Python Agent 会自动获取新工具
python -m agent.cli.main
> tools
# 会显示: calculate_weather
```

### 7.3 添加自定义推断规则

**文件**: `agent/core/agent_engine.py`

```python
def _infer_tool_calls(self, user_message: str, available_tools: list[str]):
    # 添加新工具的推断模式
    tool_patterns = {
        'calculate_weather': [
            r'\bweather\b.*\bcity\b',
            r'\btemperature\b.*\bin\b',
        ],
        # ... 其他模式
    }
    # ...
```

---

## 8. 关键发现与优化

### 8.1 已解决的问题

#### 问题 1: 本地 LLM 不遵循工具格式

**症状**: qwen2:0.5b 不会按 `TOOL: xxx\nARGUMENTS: {}` 格式输出

**解决方案**:
- 添加智能工具推断
- 支持 6 种不同的解析格式
- 在第一次迭代时自动推断并调用工具

#### 问题 2: 工具调用无限循环

**症状**: 达到最大迭代次数，工具被重复调用

**解决方案**:
- 添加 `used_tools` 集合跟踪
- 只在第一次迭代时进行推断
- 跳过已执行的工具

#### 问题 3: 空响应显示

**症状**: LLM 返回空字符串，用户看到空白

**解决方案**:
- 检查 `parsed.text` 是否为空
- 如果有工具结果，提取并清理后显示

### 8.2 系统特性

| 特性 | 说明 |
|------|------|
| 完全离线 | 使用本地 Ollama，无需网络 |
| 热插拔 | C++ 端添加工具后 Python 自动发现 |
| 多格式支持 | 支持 6+ 种工具调用格式 |
| 智能推断 | 自动识别工具调用意图 |
| 对话记忆 | 最多 100 条消息历史 |
| 美化输出 | Rich 库提供友好的 CLI |
| 异步架构 | 基于 asyncio，支持高并发 |

### 8.3 性能指标

| 指标 | 值 |
|------|-----|
| 单次工具调用 | ~50-200ms |
| 对话轮次 | 最多 5 次迭代 |
| 对话历史 | 100 条消息 |
| HTTP 超时 | 30 秒 |
| LLM 超时 | 120 秒 |

---

## 附录

### A. 快速启动指南

```bash
# 终端 1: 启动 C++ MCP Server
cd /Users/jiaran/xly/mcpServer/mcpserver/build
./McpServer

# 终端 2: 启动 Ollama (如果未运行)
ollama serve &

# 终端 3: 启动 Python Agent
cd /Users/jiaran/xly/mcpServer/mcpserver
source venv/bin/activate
python -m agent.cli.main

# 在 Agent 中测试
> What time is it now?
> Echo the word hello
> Add 15 and 27
```

### B. 支持的工具调用格式

1. 标准块格式:
```
TOOL: get_current_time
ARGUMENTS: {}
```

2. 内联代码块:
`````TOOL: get_current_time ARGUMENTS: {}```
```

3. 自然语言:
```
use get_current_time
call the get_current_time tool
```

4. 动作描述:
```
I will get the current time
let me check the time
```

5. 函数风格:
```
get_current_time()
echo(text="hello")
```

6. OpenAI 函数调用:
```
"function": {"name": "get_current_time", "arguments": {}}
```

### C. 参考资料

- [MCP 规范](https://spec.modelcontextprotocol.io/)
- [JSON-RPC 2.0](https://www.jsonrpc.org/specification)
- [Ollama 文档](https://ollama.com/)
- [Rich 文档](https://rich.readthedocs.io/)

---

**文档版本**: 1.0
**最后更新**: 2026-03-01
**作者**: Claude Code Research Agent
