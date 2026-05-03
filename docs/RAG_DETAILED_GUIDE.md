# RAG 系统详细调用流程指南

## 目录
- [1. 系统架构概览](#1-系统架构概览)
- [2. 文件上传流程](#2-文件上传流程)
- [3. 文档处理流程](#3-文档处理流程)
- [4. 用户查询流程](#4-用户查询流程)
- [5. 完整调用链路](#5-完整调用链路)
- [6. 使用示例](#6-使用示例)

---

## 1. 系统架构概览

### 1.1 核心组件

```
┌─────────────────────────────────────────────────────────────┐
│                        用户界面                              │
│                    (CLI / API / 前端)                       │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      AgentEngine                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │Conversation  │  │ RAGEngine    │  │ToolOrchestrator│     │
│  │Manager       │  │(知识检索)    │  │  (工具调用)    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
         │                   │                   │
         ▼                   ▼                   ▼
┌──────────────┐  ┌──────────────────────────────────────┐
│  OllamaClient│  │          RAG Pipeline               │
│   (LLM调用)   │  │  ┌──────────┐  ┌────────────────┐    │
└──────────────┘  │  │Chunker   │  │ ChromaDB      │    │
                  │  └──────────┘  │ (向量存储)    │    │
                  │  ┌──────────┐  └────────────────┘    │
                  │  │Embedder  │                          │
                  │  └──────────┘                          │
                  └──────────────────────────────────────┘
```

### 1.2 数据流向

**上传流向**：
```
文件 → 加载器 → 预处理器 → 分块器 → 嵌入器 → 向量数据库
```

**查询流向**：
```
用户查询 → RAG判断 → 向量检索 → 上下文构建 → LLM生成 → 返回答案
```

---

## 2. 文件上传流程

### 2.1 支持的文件类型

| 文件类型 | 扩展名 | 加载器 | 说明 |
|---------|--------|--------|------|
| 纯文本 | `.txt`, `.text` | DocumentLoader | 通用文本文件 |
| Markdown | `.md`, `.markdown` | DocumentLoader | 技术文档 |
| HTML | `.html`, `.htm` | DocumentLoader | 网页文件 |
| PDF | `.pdf` | DocumentLoader | 需要安装 pdfplumber |
| Word | `.docx` | DocumentLoader | 需要安装 python-docx |
| 网页 | `http://` | WebCrawler | 实时抓取 |
| 代码仓库 | Git URL | CodeLoader | GitHub/GitLab 等 |

### 2.2 上传方式

#### 方式 1: Python API

```python
from agent.core.agent_engine import AgentEngine
from agent.config import Config

# 启用 RAG
config = Config()
config.rag.enabled = True

# 创建 Agent
agent = await create_agent(config)

# 上传单个文件
await agent.rag_add_documents("./docs/user_manual.pdf")

# 上传整个目录
await agent.rag_add_documents("./docs/", recursive=True)

# 上传网页
await agent.rag_add_documents("https://example.com/docs")

# 上传代码仓库
await agent.rag_add_documents(
    "https://github.com/user/repo",
    languages=["python", "javascript"]
)
```

#### 方式 2: CLI 命令（待实现）

```bash
# 交互式 CLI
> /kb add ./docs/user_manual.pdf
✅ Added 15 chunks from ./docs/user_manual.pdf

> /kb add ./docs/
✅ Added 127 chunks from 5 documents

> /kb add https://example.com/api-docs
✅ Added 43 chunks from web pages
```

#### 方式 3: MCP 工具（待实现）

```json
{
  "tool": "add_documents",
  "arguments": {
    "path": "./docs/",
    "chunk_strategy": "markdown"
  }
}
```

### 2.3 文件上传详细步骤

以上传 **PDF 文档**为例：

```
用户上传 PDF (user_guide.pdf)
    │
    ▼
1. 文件路径检测
    agent/rag/loaders/document_loader.py
    ├─ 检测到 .pdf 扩展名
    ├─ 确认 pdfplumber 已安装
    └─ 选择 DocumentLoader
    │
    ▼
2. 文件内容提取
    ├─ 使用 pdfplumber 打开 PDF
    ├─ 逐页提取文本内容
    └─ 合并所有页面文本
    │
    ▼
3. 创建 Document 对象
    {
      "id": "pdf_user_guide_20260406_abc123",
      "content": "提取的文本内容...",
      "metadata": {
        "source": "./docs/user_guide.pdf",
        "type": "pdf",
        "filename": "user_guide.pdf",
        "size": 1024000
      }
    }
    │
    ▼
4. 预处理（参见第3节）
    │
    ▼
5. 返回 Document 对象
```

### 2.4 大文件处理策略

对于大文件（>10MB），系统采用分块处理：

```python
# 大文件处理流程
if file_size > 10MB:
    # 按页分批处理（PDF）
    for page_num in range(total_pages):
        page_text = extract_page(page_num)
        process_page(page_text)

    # 或按部分分割（文本文件）
    with open(file, 'r') as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break
            process_chunk(chunk)
```

---

## 3. 文档处理流程

### 3.1 处理流程图

```
Document (原始文档)
    │
    ▼
┌─────────────────────────────────────┐
│      步骤 1: 文本预处理              │
│  TextPreprocessor                   │
│  - 清理空白字符                      │
│  - 规范化引号                       │
│  - 移除URL/邮箱（可选）              │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│      步骤 2: 中文优化（可选）        │
│  ChineseTextProcessor               │
│  - 语言检测                          │
│  - jieba 分词                       │
│  - 混合语言处理                      │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│      步骤 3: 智能分块                │
│  ChunkerFactory                     │
│  ┌─────────────────────────────┐   │
│  │ 自动检测文档类型              │   │
│  │ - Markdown → MarkdownChunker│   │
│  │ - 代码 → CodeChunker         │   │
│  │ - 其他 → SemanticChunker     │   │
│  └─────────────────────────────┘   │
│                                     │
│  分块参数:                          │
│  - chunk_size: 512 tokens          │
│  - chunk_overlap: 50 tokens        │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│      步骤 4: 嵌入生成                │
│  OllamaEmbedder                     │
│  ┌─────────────────────────────┐   │
│  │ 检查缓存                      │   │
│  │  ├─ 命中 → 使用缓存          │   │
│  │  └─ 未命中 → 调用Ollama API  │   │
│  │                             │   │
│  │ POST /api/embeddings        │   │
│  │ model: nomic-embed-text      │   │
│  └─────────────────────────────┘   │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│      步骤 5: 向量存储                │
│  ChromaDB                           │
│  ┌─────────────────────────────┐   │
│  │ 创建/获取 Collection         │   │
│  │ collection_name: "kb_docs"   │   │
│  └─────────────────────────────┘   │
│                                     │
│  批量添加:                          │
│  - ids: ["chunk_001", ...]        │
│  - embeddings: [[0.1, ...], ...]   │
│  - metadatas: [{...}, ...]        │
│  - documents: ["文本...", ...]     │
└─────────────────────────────────────┘
    │
    ▼
✅ 文档处理完成，可以检索
```

### 3.2 分块示例

**原始文档**（Markdown 格式）：

```markdown
# Python 异步编程指南

## 什么是异步编程？

异步编程是一种编程范式，允许程序在等待耗时操作完成时执行其他任务。

## asyncio 模块

Python 的 asyncio 模块提供了异步编程的基础设施...
```

**分块处理**：

```python
# Chunk 1
{
  "chunk_id": "python_async_0000",
  "content": "# Python 异步编程指南\n\n## 什么是异步编程？\n\n异步编程是一种编程范式...",
  "metadata": {
    "header": "# Python 异步编程指南",
    "type": "markdown"
  },
  "token_count": 256
}

# Chunk 2
{
  "chunk_id": "python_async_0001",
  "content": "## asyncio 模块\n\nPython 的 asyncio 模块提供了异步编程的基础设施...",
  "metadata": {
    "header": "## asyncio 模块",
    "type": "markdown"
  },
  "token_count": 180
}
```

### 3.3 预处理示例

**中文优化**：

```python
# 原始文本
"向量数据库是用于存储向量数据的数据库系统"

# 处理后
# 1. 检测为中文内容
# 2. jieba 分词: ["向量", "数据库", "存储", "系统"]
# 3. 优化查询词
# 4. 生成嵌入向量
```

### 3.4 嵌入生成

```python
# 调用 Ollama API
POST http://localhost:11434/api/embeddings
Content-Type: application/json

{
  "model": "nomic-embed-text",
  "prompt": "向量数据库是用于存储向量数据的数据库系统"
}

# 响应
{
  "embedding": [
    -0.0123, 0.4804, -3.7168, ..., 0.0865
  ]
}

# 维度: 768 (nomic-embed-text)
```

---

## 4. 用户查询流程

### 4.1 查询流程时序图

```
用户输入: "什么是 asyncio？"
    │
    ▼
AgentEngine.process_message()
    │
    ▼
┌─────────────────────────────────────┐
│  步骤 1: RAG 判断                  │
│  _should_use_rag(query)            │
│                                     │
│  检测项:                            │
│  - 是否有问号?                       │
│  - 是否有疑问词? (what, how, 什么)  │
│  - 是否有知识关键词?                │
└─────────────────────────────────────┘
    │
    ├─ 需要RAG ──→ 继续
    │
    └─ 不需要 ──→ 跳到步骤 3
    │
    ▼
┌─────────────────────────────────────┐
│  步骤 2: RAG 增强查询                │
│  _rag_enhance_query(query)          │
│                                     │
│  2.1 查询优化（中文）               │
│      - jieba 分词                   │
│      - 同义词扩展                   │
│                                     │
│  2.2 向量检索                       │
│      - 生成查询嵌入                 │
│      - ChromaDB 相似度搜索          │
│      - 返回 top_k=3 个文档          │
│                                     │
│  2.3 过滤与重排序                   │
│      - 过滤低分文档                 │
│      - CrossEncoder 重排序（可选）   │
│                                     │
│  2.4 上下文构建                     │
│      - 按相关性排序                 │
│      - 生成上下文字符串             │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│  步骤 3: 构建增强消息              │
│                                     │
│  enhanced_message = """             │
│  [Context from knowledge base]     │
│  (3 relevant documents found)       │
│                                     │
│  [Source 1] (score: 0.85)          │
│  asyncio是Python的异步编程库...    │
│                                     │
│  [Source 2] (score: 0.78)          │
│  Python的asyncio模块提供...        │
│                                     │
│  User question: 什么是 asyncio？     │
│  """                                 │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│  步骤 4: ReAct 循环                 │
│                                     │
│  4.1 调用 LLM 生成回答              │
│      - 输入: enhanced_message        │
│      - 模型: qwen2:0.5b              │
│      - 输出: 基于上下文的回答        │
│                                     │
│  4.2 检测工具调用                  │
│      - parse_tool_calls()          │
│      - 检查是否需要调用工具          │
│                                     │
│  4.3 执行工具（如需要）             │
│      - execute_tool()              │
│                                     │
│  4.4 生成最终响应                  │
└─────────────────────────────────────┘
    │
    ▼
返回给用户
```

### 4.2 RAG 判断逻辑

```python
# agent/core/agent_engine.py

def _should_use_rag(self, message: str) -> bool:
    """判断是否应该使用 RAG"""

    # 1. 检查 RAG 是否启用
    if not self._rag_enabled:
        return False

    message_lower = message.lower()

    # 2. 检查问号
    if '?' in message or '？' in message:
        return True

    # 3. 检查疑问词
    question_words = [
        # 中文
        '什么', '如何', '怎么', '为什么', '哪个', '哪些',
        '是否', '有没有', '能否', '可否',
        # 英文
        'what', 'how', 'why', 'where', 'when', 'who',
        'which', 'is', 'are', 'do', 'does', 'can', 'could'
    ]
    if any(word in message_lower for word in question_words):
        return True

    # 4. 检查知识相关关键词
    knowledge_keywords = [
        'explain', 'tell me about', 'describe',
        'documentation', '说明', '解释', '介绍', '文档'
    ]
    if any(keyword in message_lower for keyword in knowledge_keywords):
        return True

    return False
```

### 4.3 查询优化示例

**用户查询**："如何使用 Python 的 asyncio 库？"

**中文优化处理**：

```python
# 1. 分词
tokens = jieba.lcut("如何使用 Python 的 asyncio 库？")
# ['如何', '使用', 'Python', '的', 'asyncio', '库', '？']

# 2. 提取关键词
keywords = ['使用', 'Python', 'asyncio', '库']

# 3. 生成查询嵌入
query_embedding = embedder.embed("如何使用 Python 的 asyncio 库？")

# 4. 向量检索
results = chromadb.query(query_embedding, top_k=3)
```

**检索结果**：

```json
{
  "sources": [
    {
      "chunk_id": "python_async_0001",
      "score": 0.853,
      "content": "Python 的 asyncio 模块提供了异步编程的基础设施...",
      "metadata": {
        "source": "./docs/python_async.md",
        "header": "## asyncio 模块"
      }
    },
    {
      "chunk_id": "python_async_0002",
      "score": 0.781,
      "content": "asyncio 是 Python 3.4 引入的标准库...",
      "metadata": {
        "source": "./docs/python_async.md",
        "header": "## 什么是异步编程？"
      }
    }
  ]
}
```

---

## 5. 完整调用链路

### 5.1 文档上传完整调用链

```python
# 用户代码
await agent.rag_add_documents("./docs/python_guide.pdf")

# 内部调用链
agent_core/agent_engine.py::rag_add_documents()
    │
    ▼
agent/rag/core/rag_engine.py::add_documents()
    │
    ├─→ agent/rag/loaders/factory.py::get_global_registry()
    │       └─→ 返回 DocumentLoader
    │
    ├─→ agent/rag/loaders/document_loader.py::load()
    │       ├─→ 检测文件类型 (.pdf)
    │       ├─→ 提取文本内容 (pdfplumber)
    │       └─→ 返回 Document 对象
    │
    ├─→ agent/rag/processors/preprocessor.py::preprocess()
    │       ├─→ 清理空白字符
    │       ├─→ 规范化引号
    │       └─→ 返回处理后的 Document
    │
    ├─→ agent/rag/processors/chinese_optimizer.py::optimize_for_search()
    │       ├─→ 检测语言类型
    │       ├─→ jieba 分词（如果是中文）
    │       └─→ 返回优化后的 Document
    │
    ├─→ agent/rag/processors/chunker.py::ChunkerFactory.auto_detect()
    │       ├─→ 检测文档类型
    │       ├─→ 选择 SemanticChunker（默认）
    │       ├─→ chunk(doc)
    │       └─→ 返回 Chunk 列表
    │
    ├─→ agent/rag/embeddings/ollama_embedder.py::embed()
    │       ├─→ 检查缓存
    │       ├─→ 调用 Ollama API (/api/embeddings)
    │       ├─→ 返回 768 维向量
    │       ├─→ 存储到缓存
    │       └─→ 返回嵌入向量
    │
    └─→ agent/rag/storage/chroma_store.py::add_documents()
            ├─→ 创建/获取 Collection
            ├─→ 批量添加到向量数据库
            ├─→ 构建 HNSW 索引
            └─→ 返回添加的文档数量
```

### 5.2 用户查询完整调用链

```python
# 用户代码
response = await agent.process_message("什么是 asyncio？")

# 内部调用链
agent_core/agent_engine.py::process_message()
    │
    ├─→ _should_use_rag("什么是 asyncio？")
    │       ├─→ 检测到问号
    │       ├─→ 检测到疑问词 "什么"
    │       └─→ 返回 True
    │
    ├─→ _rag_enhance_query("什么是 asyncio？")
    │       │
    │       ├─→ agent/rag/processors/chinese_optimizer.py
    │       │       ├─→ is_chinese(query) → True
    │       │       ├─→ optimize_for_search(query)
    │       │       └─→ 返回优化后的查询
    │       │
    │       ├─→ agent/rag/embeddings/ollama_embedder.py
    │       │       ├─→ embed(query)
    │       │       ├─→ POST /api/embeddings
    │       │       └─→ 返回查询向量 (768维)
    │       │
    │       ├─→ agent/rag/storage/chroma_store.py
    │       │       ├─→ search(query, top_k=3)
    │       │       ├─→ ChromaDB 向量相似度搜索
    │       │       ├─→ 返回 Top 3 最相似文档
    │       │       └─→ 返回 SearchResult 列表
    │       │
    │       └─→ 返回 RAG 上下文字符串
    │
    ├─→ 构建增强消息
    │       enhanced_message = f"{rag_context}\n\nUser: {query}"
    │
    ├─→ conversation.add_user_message(enhanced_message)
    │
    └─→ _process_with_tools()
            │
            ├─→ conversation.get_messages_for_llm()
            │
            ├─→ agent/llm/ollama_client.py::chat()
            │       ├─→ POST /api/chat
            │       ├─→ 模型生成回答（基于 RAG 上下文）
            │       └─→ 返回回答文本
            │
            ├─→ agent/llm/response_parser.py::parse_tool_calls()
            │       ├─→ 解析工具调用（JSON 格式）
            │       └─→ 返回 ParsedResponse
            │
            └─→ 返回最终响应
```

### 5.3 核心文件映射表

| 功能 | 文件路径 | 核心方法/类 |
|------|----------|-------------|
| RAG 判断 | `agent/core/agent_engine.py` | `_should_use_rag()` |
| RAG 增强 | `agent/core/agent_engine.py` | `_rag_enhance_query()` |
| 文档加载 | `agent/rag/loaders/document_loader.py` | `DocumentLoader.load()` |
| 网页抓取 | `agent/rag/loaders/web_crawler.py` | `WebCrawler.load()` (待实现) |
| 代码加载 | `agent/rag/loaders/code_loader.py` | `CodeLoader.load()` (待实现) |
| 文本预处理 | `agent/rag/processors/preprocessor.py` | `TextPreprocessor.preprocess()` |
| 中文优化 | `agent/rag/processors/chinese_optimizer.py` | `ChineseTextProcessor.*` |
| 文档分块 | `agent/rag/processors/chunker.py` | `*Chunker.chunk()` |
| 嵌入生成 | `agent/rag/embeddings/ollama_embedder.py` | `OllamaEmbedder.embed()` |
| 向量存储 | `agent/rag/storage/chroma_store.py` | `ChromaVectorStore.*` |
| 查询处理 | `agent/rag/core/rag_engine.py` | `RAGEngine.query()` |
| 配置管理 | `agent/config.py` | `RagConfig` |

---

## 6. 使用示例

### 6.1 完整示例：上传 PDF 并查询

```python
import asyncio
from agent.core.agent_engine import create_agent
from agent.config import Config

async def main():
    # 1. 创建配置（启用 RAG）
    config = Config()
    config.rag.enabled = True
    config.rag.embedder_model = "nomic-embed-text"
    config.rag.chunk_size = 512
    config.rag.top_k = 3

    # 2. 创建 Agent
    agent = await create_agent(config)

    # 3. 上传 PDF 文档
    print("上传文档中...")
    chunks = await agent.rag_add_documents(
        source="./docs/python_async_guide.pdf",
        chunk_strategy="auto"  # 自动检测分块策略
    )
    print(f"✅ 上传完成，创建了 {chunks} 个文档块")

    # 4. 查询知识库
    print("\n发送查询...")
    response = await agent.process_message("asyncio 是如何工作的？")

    # 5. 查看结果
    print(f"\n回答:\n{response}")

    # 6. 查看 RAG 统计
    stats = agent.get_rag_stats()
    print(f"\nRAG 统计:")
    print(f"  文档总数: {stats['collection_info']['count']}")
    print(f"  查询次数: {stats['usage_stats']['queries_processed']}")

asyncio.run(main())
```

### 6.2 批量上传多个文件

```python
# 上传多个 PDF 文档
import asyncio
from pathlib import Path

async def batch_upload():
    agent = await create_agent(config)

    docs_path = Path("./docs")
    pdf_files = list(docs_path.glob("*.pdf"))

    print(f"找到 {len(pdf_files)} 个 PDF 文件")

    total_chunks = 0
    for pdf_file in pdf_files:
        print(f"处理 {pdf_file.name}...")
        chunks = await agent.rag_add_documents(str(pdf_file))
        total_chunks += chunks
        print(f"  ✅ {pdf_file.name}: {chunks} chunks")

    print(f"\n总计: {total_chunks} 个文档块已添加")

asyncio.run(batch_upload())
```

### 6.3 中文查询示例

```python
# 中文查询
response1 = await agent.process_message("Python 中的异步编程是什么？")
# 系统会：
# 1. 检测到中文问题
# 2. 使用 jieba 优化查询
# 3. 检索相关知识库
# 4. 生成中文回答

response2 = await agent.process_message("How to use asyncio in Python?")
# 英文查询同样支持
```

### 6.4 增量更新知识库

```python
# 添加新文档
await agent.rag_add_documents("./docs/new_feature.pdf")

# 查询 RAG 统计
stats = agent.get_rag_stats()
print(f"当前文档数: {stats['collection_info']['count']}")

# 清空知识库（如果需要）
await agent.rag_clear()
print("知识库已清空")
```

---

## 7. 故障排查

### 7.1 文档上传失败

**问题**：PDF 上报错 "pdfplumber not installed"

**解决**：
```bash
pip install pdfplumber
```

**问题**：PDF 乱码

**解决**：
- 检查 PDF 是否为扫描版（图片格式）
- 尝试使用 OCR 工具预先处理

### 7.2 查询无结果

**问题**：查询返回 "I couldn't find relevant information"

**可能原因**：
1. 文档未正确上传
2. 查询与文档内容差异较大
3. score_threshold 设置过高

**排查步骤**：
```python
# 1. 检查文档是否已上传
stats = agent.get_rag_stats()
print(f"文档数: {stats['collection_info']['count']}")

# 2. 降低阈值
config.rag.score_threshold = 0.3

# 3. 增加 top_k
config.rag.top_k = 10

# 4. 查看原始检索结果
result = await agent.rag_query("你的查询")
for source in result.sources:
    print(f"Score: {source.score:.3f}, Content: {source.document.content[:100]}...")
```

### 7.3 中文查询效果差

**问题**：中文查询结果不准确

**解决**：
```bash
# 确保安装 jieba
pip install jieba

# 启用中文优化
config.rag.enable_chinese_optimization = True
```

---

## 8. 性能优化建议

### 8.1 上传优化

- **批量上传**：一次上传多个文件，减少初始化开销
- **增量更新**：只上传变更的文档
- **分块大小**：根据文档类型调整 chunk_size
  - 代码：256-512 tokens
  - Markdown：512-1024 tokens
  - 长文本：1024-2048 tokens

### 8.2 查询优化

- **启用缓存**：`config.rag.enable_cache = True`
- **调整 top_k**：根据需求设置（3-10）
- **使用重排序**：对于重要查询启用 reranker（待实现）

---

## 9. 总结

### 核心流程回顾

**上传流程**：
```
文件 → 加载器 → 预处理 → 分块 → 嵌入 → ChromaDB
```

**查询流程**：
```
用户查询 → RAG判断 → 检索优化 → 向量检索 → 上下文构建 → LLM生成
```

### 关键点

1. **文档上传**：自动检测文件类型，选择合适的加载器
2. **智能分块**：根据文档类型选择最佳分块策略
3. **中文优化**：jieba 分词 + 混合语言处理
4. **向量检索**：ChromaDB + 余弦相似度
5. **无缝集成**：与 Agent ReAct 循环完美集成

### 下一步

- 实现网页抓取加载器（WebCrawler）
- 实现代码仓库加载器（CodeLoader）
- 添加混合检索（向量 + BM25）
- 实现 CrossEncoder 重排序
- 添加对话历史和反馈学习