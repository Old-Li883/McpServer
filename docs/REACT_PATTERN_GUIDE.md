# MCP Agent ReAct模式核心代码指南

## 核心代码位置

### 主引擎文件
**文件**: `agent/core/agent_engine.py`

### ReAct循环核心方法

#### 1. 主入口：`process_message()` (第66-89行)

```python
async def process_message(
    self,
    user_message: str,
    max_tool_iterations: int = 5,
) -> str:
    """处理用户消息并生成响应"""
    # 添加用户消息到对话历史
    self.conversation.add_user_message(user_message)

    # 处理潜在的工具调用
    response = await self._process_with_tools(max_tool_iterations, user_message)

    return response
```

#### 2. ReAct核心循环：`_process_with_tools()` (第91-187行)

这是**ReAct模式的核心实现**，包含完整的循环逻辑：

```python
while iteration < max_iterations:
    # 1. Observe (观察)
    messages = self.conversation.get_messages_for_llm()

    # 2. Think (思考)
    llm_response = await self._generate_response(messages)
    parsed = self.tool_orchestrator.parse_tool_calls(llm_response, ...)

    # 3. 工具推断 (可选)
    if not parsed.has_tool_calls and iteration == 1:
        inferred_calls = self._infer_tool_calls(user_message, ...)
        parsed.tool_calls.extend(inferred_calls)

    # 4. Act (行动)
    if not parsed.tool_calls:
        # 无工具调用，返回响应
        final_response = parsed.text
        break

    # 执行工具调用
    for tool_call in parsed.tool_calls:
        result = await self.tool_orchestrator.execute_tool(...)
        self.conversation.add_tool_message(tool_call.name, result)
```

## ReAct模式详解

### ReAct = Reasoning (推理) + Acting (行动)

ReAct是一种结合推理和行动的AI代理模式，通过迭代循环来完成任务。

### 循环阶段

#### 1. Observe (观察)
- **代码位置**: `agent_engine.py:113`
- **功能**: 获取当前对话上下文
- **实现**: `conversation.get_messages_for_llm()`
- **返回**: 包含系统提示、用户消息、助手响应、工具结果的消息列表

#### 2. Think (思考)
- **代码位置**: `agent_engine.py:116-120`
- **功能**: 使用LLM生成响应和决策
- **实现**:
  - `_generate_response()`: 调用Ollama API
  - `parse_tool_calls()`: 解析工具调用
- **输出**: 解析后的响应（文本 + 工具调用列表）

#### 3. Act (行动)
- **代码位置**: `agent_engine.py:158-182`
- **功能**: 执行工具或返回响应
- **实现**: `tool_orchestrator.execute_tool()`
- **流程**:
  1. 检查是否有工具调用
  2. 如果无工具调用，返回文本响应
  3. 如果有工具调用，执行并继续循环

### 工具推断机制

**代码位置**: `agent_engine.py:208-276`

当LLM没有调用工具时，使用正则表达式从用户消息推断需要调用的工具。

支持的工具推断：
- `get_current_time`: 时间查询
- `echo`: 文本重复
- `add`: 加法计算

### 关键特性

#### 1. 迭代限制
- 默认最大迭代次数: 5
- 防止无限循环
- 可通过 `max_tool_iterations` 参数调整

#### 2. 工具去重
- **代码位置**: `agent_engine.py:107, 159-161`
- 使用 `used_tools` 集合跟踪已使用的工具
- 避免重复执行相同工具

#### 3. 对话管理
- **代码位置**: `agent/core/conversation.py`
- 维护完整的对话历史
- 支持多轮对话
- 限制历史长度（默认100条）

#### 4. 错误处理
- 工具执行错误被捕获并添加到对话
- LLM可以看到错误信息并决定下一步
- 不会因单个工具失败而中断整个流程

## 相关组件

### ToolOrchestrator
**文件**: `agent/core/tools.py`

- 加载MCP工具
- 解析工具调用
- 执行工具调用
- 格式化工具结果

### Conversation
**文件**: `agent/core/conversation.py`

- 管理对话历史
- 支持多种消息角色
- 提供LLM格式的消息列表

### OllamaClient
**文件**: `agent/llm/ollama_client.py`

- 连接到Ollama API
- 支持流式和非流式响应
- 处理连接错误

### McpClient
**文件**: `agent/mcp/client.py`

- 连接到MCP Server
- 调用远程工具
- 访问资源和提示词

## PlantUML流程图

项目包含两个详细的PlantUML图：

1. **流程图**: `docs/08_react_flow_diagram.puml`
   - 展示ReAct循环的控制流
   - 包含决策分支和条件判断

2. **时序图**: `docs/09_react_sequence_diagram.puml`
   - 展示组件之间的交互
   - 包含完整的消息传递过程

## 使用示例

```python
# 创建并初始化agent
agent = await create_agent(config)

# 处理用户消息（内部执行ReAct循环）
response = await agent.process_message("现在几点了？")
# 循环:
# 1. Observe: 获取消息上下文
# 2. Think: LLM决定调用 get_current_time
# 3. Act: 执行工具获取时间
# 4. Observe: 添加工具结果到上下文
# 5. Think: LLM基于时间生成最终响应
# 6. Act: 返回响应给用户

print(response)  # "现在是2026年4月5日 01:45"
```

## 调试建议

### 启用详细日志
```python
# 在 config.yaml 中设置
agent:
  log_level: "DEBUG"
```

### 查看对话历史
```python
history = agent.get_conversation_history()
for msg in history:
    print(f"{msg.role}: {msg.content}")
```

### 监控工具调用
```bash
# 查看MCP Server日志
tail -f logs/mcpserver.log | grep "Tool called"
```

## 扩展ReAct模式

### 添加新的工具推断
编辑 `_infer_tool_calls()` 方法，添加新的正则表达式模式。

### 自定义迭代策略
修改 `_process_with_tools()` 的循环条件，实现更复杂的终止条件。

### 增强工具结果处理
在工具执行后添加结果验证和后处理逻辑。
