# McpServer

基于 Model Context Protocol (MCP) 的双语言 AI Agent 系统，由 C++ 高性能 MCP 协议服务器和 Python 智能代理组成。支持本地大模型（Ollama）推理、RAG 文档检索和跨会话记忆。

## 架构概览

```
┌─────────────────────────────────────────────────────────┐
│                    Python Agent                          │
│  ┌──────────┐  ┌──────────┐  ┌───────┐  ┌────────────┐ │
│  │ CLI 交互  │  │ ReAct引擎 │  │  RAG  │  │   Memory   │ │
│  │prompt-kit │  │ LLM 调用  │  │ 文档检索│  │ 短期+长期  │ │
│  └─────┬────┘  └─────┬────┘  └───────┘  └────────────┘ │
│        └──────────────┼──────────────────────────────────┤
│                  MCP Client (HTTP)                       │
└───────────────────────┬─────────────────────────────────┘
                        │ JSON-RPC 2.0
┌───────────────────────┼─────────────────────────────────┐
│                  C++ MCP Server                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │   Tools   │  │Resources │  │ Prompts  │              │
│  └──────────┘  └──────────┘  └──────────┘              │
│  ┌──────────┐  ┌──────────┐                             │
│  │   HTTP    │  │  Stdio   │                             │
│  │ Transport │  │Transport │                             │
│  └──────────┘  └──────────┘                             │
└──────────────────────────────────────────────────────────┘
```

## 功能特性

- **MCP 协议服务器** (C++)：完整的 JSON-RPC 2.0 实现，支持 HTTP 和 Stdio 双传输模式
- **工具管理**：动态注册/调用 Tool，支持自定义输入 Schema
- **资源管理**：MCP Resources 的注册与读取
- **Prompt 管理**：MCP Prompts 模板管理
- **AI 代理** (Python)：ReAct 循环，自动调用 MCP 工具完成任务
- **本地推理**：集成 Ollama，支持任意本地大模型
- **RAG 检索增强**：文档加载（PDF/HTML/DOCX/TXT）、向量嵌入、ChromaDB 存储、相似度检索
- **记忆系统**：短期对话记忆（带摘要压缩）+ 长期向量记忆（跨会话持久化）

## 快速开始

### 环境要求

- macOS (Apple Silicon)
- CMake 3.15+, C++17 编译器
- Python 3.10+
- [Ollama](https://ollama.ai)（用于本地 LLM 推理）

### 安装依赖

```bash
# C++ 依赖
brew install nlohmann-json spdlog fmt cpp-httplib openssl googletest

# Python 依赖
python3 -m venv venv
source venv/bin/activate
pip install -e agent/.[dev,rag]
```

### 构建 & 运行

```bash
# 构建 C++ 服务器
cmake -B build -S .
cmake --build build

# 启动 MCP 服务器
./build/McpServer

# 启动 Python Agent（另一个终端）
source venv/bin/activate
python -m agent.cli.main
```

服务器默认监听 `http://0.0.0.0:8080`，可在 `config.json` 中修改。

## 运行测试

```bash
# 运行全部测试（C++ + Python）
bash scripts/run_all_tests.sh

# 仅 C++ 测试
cd build && ctest --output-on-failure

# 仅 Python 测试
source venv/bin/activate
pytest tests/ agent/llm/tests/ -v
```

项目配置了 Git pre-push hook，push 前自动运行全部测试。安装方式：

```bash
bash scripts/install_hooks.sh
```

紧急跳过：`SKIP_TESTS=1 git push`

## 项目结构

```
├── src/                          # C++ 源码
│   ├── mcp/                      # MCP 协议层
│   │   ├── McpServer             # 协议处理器
│   │   ├── McpServerRunner       # 统一入口
│   │   ├── ToolManager           # 工具管理
│   │   ├── ResourceManager       # 资源管理
│   │   └── PromptManager         # Prompt 管理
│   ├── json_rpc/                 # JSON-RPC 2.0 层
│   │   ├── JsonRpc               # 请求解析与分发
│   │   ├── HttpJsonRpcServer     # HTTP 传输
│   │   └── StdioJsonRpc          # Stdio 传输
│   └── logger/                   # 日志模块
├── agent/                        # Python Agent
│   ├── cli/                      # CLI 交互界面
│   ├── core/                     # ReAct 引擎
│   ├── mcp/                      # MCP 客户端
│   ├── llm/                      # Ollama 集成
│   ├── rag/                      # RAG 检索增强
│   │   ├── loaders/              # 文档加载器
│   │   ├── embeddings/           # 向量嵌入
│   │   ├── storage/              # ChromaDB 存储
│   │   └── core/                 # 检索引擎
│   └── memory/                   # 记忆系统
│       ├── short_term/           # 对话历史
│       └── long_term/            # 向量长期记忆
├── tests/                        # C++ & Python 测试
├── config/                       # 配置模块
├── scripts/                      # 自动化脚本
├── CMakeLists.txt                # C++ 构建配置
├── vcpkg.json                    # C++ 依赖声明
└── agent/pyproject.toml          # Python 包配置
```

## 技术栈

| 层 | 技术 |
|---|------|
| C++ 服务器 | C++17, nlohmann/json, cpp-httplib, spdlog, OpenSSL |
| Python Agent | httpx, Pydantic, Ollama, ChromaDB, Rich, Prompt Toolkit |
| 构建工具 | CMake, vcpkg, hatchling |
| 测试框架 | Google Test (C++), pytest + pytest-asyncio (Python) |

## 配置

- **C++ 服务器**：`config.json`（端口、模式、日志级别）
- **Python Agent**：`agent/config.yaml`（MCP 服务器地址、Ollama 设置、RAG/记忆配置、系统提示词）

## License

MIT
