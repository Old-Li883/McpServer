# RAG 系统使用指南

## 概述

RAG (Retrieval-Augmented Generation) 系统已成功集成到 MCP Agent 中，提供以下功能：

- ✅ 智能文档分块（语义分块、Markdown分块、代码分块）
- ✅ 中文文本优化（jieba 分词、混合语言处理）
- ✅ 向量存储和检索（ChromaDB）
- ✅ 与 Agent Engine 集成

## 配置

在 `agent/config.yaml` 或配置文件中添加 RAG 配置：

```yaml
rag:
  enabled: true                    # 启用 RAG
  embedder_model: "nomic-embed-text"  # Ollama 嵌入模型
  vector_db_path: "./data/chroma"     # 向量数据库路径
  collection_name: "knowledge_base"   # 集合名称
  chunk_size: 512                     # 分块大小
  chunk_overlap: 50                   # 分块重叠
  top_k: 5                            # 检索文档数
  score_threshold: 0.5                # 相似度阈值
  enable_chinese_optimization: true   # 中文优化
```

## 使用方法

### 1. Python API

```python
from agent.rag import RAGEngine, RAGConfig

# 创建配置
config = RAGConfig(
    embedder_model="nomic-embed-text",
    vector_db_path="./data/chroma",
    chunk_size=512,
)

# 初始化引擎
async with RAGEngine(config) as rag:
    # 添加文档
    chunks_added = await rag.add_documents("./docs")
    print(f"Added {chunks_added} chunks")

    # 查询知识库
    result = await rag.query("什么是向量数据库？")
    print(result.answer)
    print(f"Sources: {result.source_count}")
```

### 2. 分块策略

```python
from agent.rag.processors.chunker import ChunkerFactory

# 自动检测分块器
chunker = ChunkerFactory.auto_detect(document)

# 或手动指定
chunker = ChunkerFactory.create(
    strategy="semantic",  # fixed, semantic, markdown, code
    chunk_size=512,
    chunk_overlap=50,
)

# 执行分块
chunks = chunker.chunk(document)
```

### 3. Agent 集成

```python
from agent.core.agent_engine import AgentEngine
from agent.config import Config

# 配置 RAG
config = Config()
config.rag.enabled = True

# 创建 Agent
agent = await create_agent(config)

# 添加知识库文档
await agent.rag_add_documents("./docs")

# 查询时会自动使用 RAG
response = await agent.process_message("什么是向量数据库？")
```

## 分块策略说明

| 策略 | 描述 | 适用场景 |
|------|------|----------|
| `fixed` | 固定长度分块 | 通用文档 |
| `semantic` | 语义分块（保持句子完整） | 文章、博客 |
| `markdown` | 保留 Markdown 结构 | 技术文档 |
| `code` | 按函数/类分块 | 代码文件 |

## 中文优化

启用中文优化后，系统会：

- 使用 jieba 进行智能分词
- 识别中英文混合内容
- 优化检索质量
- 提供更好的上下文构建

## 已实现功能

### ✅ Phase 1: 基础框架
- Document 数据类型
- 基础 Loader 接口
- Ollama 嵌入器（带缓存）
- ChromaDB 集成

### ✅ 环节 1: 文档分块
- FixedChunker
- SemanticChunker
- MarkdownChunker
- CodeChunker
- ChunkerFactory

### ✅ 预处理和中文优化
- TextPreprocessor
- MarkdownPreprocessor
- CodePreprocessor
- ChineseTextProcessor
- ChineseSentenceSplitter

### ✅ Agent 集成
- AgentEngine RAG 集成
- 配置管理
- 智能路由
- RAG 查询方法

## 测试

运行测试脚本：

```bash
python -m agent.examples.test_rag_with_agent
```

## 下一步

可以继续实现：

1. **环节 2**: 网页抓取加载器 (WebCrawler)
2. **环节 3**: 代码仓库加载器 (CodeLoader)
3. **环节 4**: 智能检索层（混合检索、重排序）
4. **集成点 1**: MCP 工具集成
