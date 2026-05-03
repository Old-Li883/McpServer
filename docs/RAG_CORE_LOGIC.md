# RAG 核心逻辑解析：从文档上传到向量存储

## 核心流程概览

```
原始文档 → 分块 → 嵌入生成 → 向量存储
    ↓        ↓        ↓        ↓
  1个文档  N个chunk  N个向量  N条记录
```

---

## 1. 文档分块核心逻辑

### 1.1 分块流程图

```
Document (原始文档)
    ↓
[ChunkerFactory] 自动检测文档类型
    ↓
    ├─ Markdown → MarkdownChunker
    ├─ 代码文件 → CodeChunker
    └─ 其他 → SemanticChunker
    ↓
Chunk[] (文档块列表)
```

### 1.2 核心代码位置

**文件**: `agent/rag/processors/chunker.py`

#### 关键类和方法

```python
# 1. 分块工厂类
class ChunkerFactory:
    @staticmethod
    def auto_detect(document: Document, **kwargs) -> BaseChunker:
        """自动检测并创建合适的分块器"""
        doc_type = document.metadata.get("type", "text")
        
        if doc_type == "markdown":
            return MarkdownChunker(**kwargs)
        elif doc_type == "code":
            language = _detect_language(filename)
            return CodeChunker(language=language, **kwargs)
        else:
            return SemanticChunker(**kwargs)

# 2. 语义分块器实现
class SemanticChunker(BaseChunker):
    def chunk(self, document: Document) -> List[Chunk]:
        """核心分块逻辑"""
        # 步骤1: 按句子分割
        sentences = self._split_sentences(document.content)
        
        # 步骤2: 累积句子直到达到 chunk_size
        current_chunk = ""
        chunks = []
        
        for sentence in sentences:
            test_chunk = current_chunk + "\n" + sentence
            
            # 步骤3: 检查 token 数量
            if self._estimate_tokens(test_chunk) <= self.chunk_size:
                current_chunk = test_chunk
            else:
                # 步骤4: 保存当前块
                if len(current_chunk) >= self.min_chunk_size:
                    chunks.append(self._create_chunk(current_chunk))
                
                # 步骤5: 开始新块（保留重叠部分）
                overlap_sentences = self._get_overlap_sentences(...)
                current_chunk = "\n".join(overlap_sentences + [sentence])
        
        return chunks

# 3. Token 估算
def _estimate_tokens(self, text: str) -> int:
    """估算 token 数量"""
    # 中文: 1 token ≈ 2 字符
    # 英文: 1 token ≈ 4 字符
    chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', text))
    other_chars = len(text) - chinese_chars
    
    return (chinese_chars // 2) + (other_chars // 4)
```

### 1.3 分块策略对比

| 分块器 | 适用场景 | 分块边界 | 保留结构 |
|--------|----------|----------|----------|
| **FixedChunker** | 通用文档 | 固定长度 | ❌ |
| **SemanticChunker** | 文章、博客 | 句子边界 | ✅ |
| **MarkdownChunker** | 技术文档 | 标题层级 | ✅ |
| **CodeChunker** | 代码 | 函数/类 | ✅ |

### 1.4 分块示例

**输入文档**：
```
Python 是一种高级编程语言。它支持多种编程范式，
包括面向对象、函数式和过程式编程。
```

**分块结果** (chunk_size=256)：

```
Chunk 1: "Python 是一种高级编程语言。它支持多种编程范式"
  - token_count: 23
  - chunk_id: "doc_001_chunk_0000"

Chunk 2: "包括面向对象、函数式和过程式编程"
  - token_count: 18
  - chunk_id: "doc_001_chunk_0001"
```

---

## 2. 向量生成核心逻辑

### 2.1 嵌入生成流程

```
Chunk (文档块)
    ↓
[检查缓存]
    ├─ 命中 → 返回缓存的向量
    └─ 未命中 → 调用 Ollama API
    ↓
[OllamaEmbedder.embed()]
    ↓
POST http://localhost:11434/api/embeddings
{
  "model": "nomic-embed-text",
  "prompt": "chunk 文本内容"
}
    ↓
768维向量
[-0.012, 0.480, -3.716, ..., 0.086]
    ↓
[存储到缓存]
    ↓
返回向量
```

### 2.2 核心代码位置

**文件**: `agent/rag/embeddings/ollama_embedder.py`

```python
class OllamaEmbedder:
    async def embed(self, text: str) -> List[float]:
        """生成嵌入向量的核心方法"""
        
        # 1. 检查缓存
        if self.enable_cache:
            cached = self._get_from_cache(text)
            if cached is not None:
                return cached  # 缓存命中，直接返回
        
        # 2. 调用 Ollama API
        embedding = await self._generate_embedding(text)
        
        # 3. 存储到缓存
        if self.enable_cache:
            self._store_in_cache(text, embedding)
        
        return embedding
    
    async def _generate_embedding(self, text: str) -> List[float]:
        """调用 Ollama API 生成向量"""
        response = await self.client.post(
            "/api/embeddings",  # ← 关键：API 端点
            json={
                "model": self.model,  # ← 模型名称
                "prompt": text,      # ← 文本内容
            }
        )
        response.raise_for_status()
        data = response.json()
        
        # 返回 768 维向量
        return data["embedding"]
```

### 2.3 API 调用示例

**请求**：
```bash
curl http://localhost:11434/api/embeddings -d '{
  "model": "nomic-embed-text",
  "prompt": "Python 是一种编程语言"
}'
```

**响应**：
```json
{
  "embedding": [
    -0.012327785603702068,
    0.48047110438346863,
    -3.7168688774108887,
    0.44570955634117126,
    ...
    0.08651404082775116
  ]
}
```

### 2.4 缓存机制

```python
# 缓存键生成
def _get_cache_key(self, text: str) -> str:
    """根据文本内容生成缓存键"""
    content = f"{self.model}:{text}"
    return hashlib.sha256(content.encode()).hexdigest()

# 缓存结构
{
  "abc123def456": [  # cache_key
    -0.012, 0.480, ...
  ]
}
```

**优点**：
- 相同文本只计算一次
- 显著提升性能
- 减少 API 调用

---

## 3. 向量存储核心逻辑

### 3.1 存储流程

```
Chunk[] (带向量的文档块)
    ↓
[ChromaVectorStore.add_documents()]
    ↓
批量处理：
  for chunk in chunks:
    - chunk.id           → ids
    - chunk.embedding    → embeddings
    - chunk.metadata     → metadatas
    - chunk.content      → documents
    ↓
[ChromaDB Collection.add()]
    ↓
构建 HNSW 索引（余弦相似度）
    ↓
持久化到磁盘
```

### 3.2 核心代码位置

**文件**: `agent/rag/storage/chroma_store.py`

```python
class ChromaVectorStore:
    async def add_documents(
        self,
        documents: List[Document]
    ) -> None:
        """批量添加文档到向量数据库"""
        
        # 1. 准备数据
        ids = []
        embeddings = []
        metadatas = []
        contents = []
        
        for doc in documents:
            ids.append(doc.id)
            embeddings.append(doc.embedding)  # ← 已生成的向量
            metadatas.append(self._flatten_metadata(doc.metadata))
            contents.append(doc.content)
        
        # 2. 添加到 ChromaDB 集合
        self._collection.add(
            ids=ids,
            embeddings=embeddings,     # ← 关键：向量数据
            metadatas=metadatas,       # ← 元数据
            documents=contents,        # ← 原始文本
        )
```

### 3.3 数据结构

**存储在 ChromaDB 中的记录**：

```python
{
    "id": "doc_001_chunk_0000",
    
    "embedding": [          # 768维向量
        -0.012, 0.480, ...
    ],
    
    "metadata": {           # 扁平化的元数据
        "source": "./docs/guide.pdf",
        "type": "pdf",
        "chunk_id": "doc_001_chunk_0000",
        "token_count": 256
    },
    
    "document": "Python 是一种高级编程语言..."
}
```

### 3.4 向量索引

**HNSW (Hierarchical Navigable Small World)** 算法：

```
┌─────────────────────────────────┐
│      高维向量空间 (768维)         │
├─────────────────────────────────┤
│                                 │
│  vec1 ●                        │
│        ╲  ╱                     │
│         ╲╱  ← 相似度高             │
│    vec2 ●● vec3                 │
│         ╳╳                      │
│    vec4 ●                        │
│                                 │
└─────────────────────────────────┘

● = 文档向量
╲╱ = 近邻连接 (HNSW 图)
```

**相似度计算**：

```python
# 余弦相似度
def cosine_similarity(vec1, vec2):
    dot_product = np.dot(vec1, vec2)
    norm1 = np.linalg.norm(vec1)
    norm2 = np.linalg.norm(vec2)
    return dot_product / (norm1 * norm2)

# ChromaDB 使用余弦距离
distance = 1 - cosine_similarity
```

---

## 4. 完整流程代码追踪

### 4.1 入口：上传文档

```python
# agent/rag/core/rag_engine.py

async def add_documents(
    self,
    source: str,
    **kwargs
) -> int:
    """添加文档到知识库"""
    
    # 1. 加载文档
    documents = []
    async for doc in self._loader_registry.load_async(source, **kwargs):
        documents.append(doc)
    
    # 2. 预处理 + 分块 + 嵌入
    all_chunks = []
    for doc in documents:
        # 预处理
        content = self._preprocessor.preprocess(doc.content)
        processed_doc = Document(id=doc.id, content=content, ...)
        
        # 分块
        chunker = ChunkerFactory.auto_detect(processed_doc)
        chunks = chunker.chunk(processed_doc)
        
        # 为每个 chunk 生成嵌入
        for chunk in chunks:
            chunk_doc = chunk.to_document()
            # 生成向量（在 add_documents 时完成）
            all_chunks.append(chunk_doc)
    
    # 3. 存储到向量数据库
    await self._vector_store.add_documents(all_chunks)
    
    return len(all_chunks)
```

### 4.2 关键数据流转

```
原始文档 (1个)
    │
    ├─ 内容: "Python 是..." (1000字符)
    ├─ 元数据: {"source": "guide.pdf", "type": "text"}
    │
    ▼
[预处理]
    │
    ├─ 内容: "Python 是..." (1000字符，已清洗)
    ├─ 元数据: 添加 "processed": true
    │
    ▼
[分块] (假设 chunk_size=256)
    │
    ├─ Chunk 0: tokens=234, content="..."
    ├─ Chunk 1: tokens=256, content="..."
    ├─ Chunk 2: tokens=189, content="..."
    ├─ Chunk 3: tokens=156, content="..."
    │
    ▼
[嵌入生成] (对每个 chunk)
    │
    ├─ Chunk 0 → embedding[768]
    ├─ Chunk 1 → embedding[768]
    ├─ Chunk 2 → embedding[768]
    ├─ Chunk 3 → embedding[768]
    │
    ▼
[向量存储]
    │
    ├─ Record 0: {id, embedding, metadata, content}
    ├─ Record 1: {id, embedding, metadata, content}
    ├─ Record 2: {id, embedding, metadata, content}
    ├─ Record 3: {id, embedding, metadata, content}
    │
    ▼
✅ 存储完成，可检索
```

---

## 5. 核心逻辑代码位置速查表

| 步骤 | 文件 | 核心类/方法 | 行号参考 |
|------|------|-------------|----------|
| **分块** | `agent/rag/processors/chunker.py` | `SemanticChunker.chunk()` | ~140-200 |
| **嵌入生成** | `agent/rag/embeddings/ollama_embedder.py` | `OllamaEmbedder.embed()` | ~78-100 |
| **API调用** | `agent/rag/embeddings/ollama_embedder.py` | `_generate_embedding()` | ~117-132 |
| **缓存管理** | `agent/rag/embeddings/ollama_embedder.py` | `_get_from_cache()` | ~139-145 |
| **向量存储** | `agent/rag/storage/chroma_store.py` | `add_documents()` | ~60-80 |
| **集合操作** | `agent/rag/storage/chroma_store.py` | `_collection.add()` | ChromaDB |

---

## 6. 关键数据结构

### 6.1 Document → Chunk 转换

```python
# 输入: Document
{
    "id": "doc_001",
    "content": "完整的文档内容...",
    "metadata": {"source": "guide.pdf"}
}

# 输出: Chunk
{
    "content": "文档的第1部分内容...",
    "doc_id": "doc_001",
    "chunk_id": "doc_001_chunk_0000",
    "metadata": {...},
    "token_count": 256
}

# Chunk → Document (用于存储)
chunk_doc = chunk.to_document()
{
    "id": "doc_001_chunk_0000",
    "content": "文档的第1部分内容...",
    "metadata": {
        "doc_id": "doc_001",
        "chunk_id": "doc_001_chunk_0000",
        "token_count": 256
    },
    "embedding": [0.012, -0.234, ...]  # ← 稍后添加
}
```

### 6.2 向量数据库存储格式

```python
# ChromaDB 内部格式
{
    "ids": ["doc_001_chunk_0000", "doc_001_chunk_0001", ...],
    
    "embeddings": [
        [-0.012, 0.480, ...],  # 768维
        [0.234, -0.567, ...],  # 768维
        ...
    ],
    
    "metadatas": [
        {"source": "guide.pdf", "chunk_id": "...", ...},
        {"source": "guide.pdf", "chunk_id": "...", ...},
        ...
    ],
    
    "documents": [
        "文档的第1部分内容...",
        "文档的第2部分内容...",
        ...
    ]
}
```

---

## 7. 性能优化点

### 7.1 分块优化

```python
# 根据文档类型选择最佳分块策略
if doc_type == "code":
    chunk_size = 256  # 代码块较小
elif doc_type == "markdown":
    chunk_size = 1024  # Markdown 可以更大
else:
    chunk_size = 512  # 默认
```

### 7.2 嵌入优化

```python
# 批量处理（减少 API 调用）
embeddings = await embedder.embed_batch([
    chunk1.content,
    chunk2.content,
    chunk3.content
])  # 一次调用，返回3个向量
```

### 7.3 存储优化

```python
# 批量添加（提升性能）
await vector_store.add_documents(all_chunks)
# 而不是
for chunk in chunks:
    await vector_store.add_documents([chunk])  # 慢很多
```

---

## 总结

**核心三步曲**：

1. **分块** (`chunker.py`)
   - 输入：1个 Document
   - 输出：N 个 Chunk
   - 核心：按语义边界切分

2. **嵌入** (`ollama_embedder.py`)
   - 输入：文本内容
   - 输出：768维向量
   - 核心：调用 Ollama API

3. **存储** (`chroma_store.py`)
   - 输入：带向量的 Chunk
   - 输出：持久化到向量数据库
   - 核心：ChromaDB HNSW 索引

**关键**：每一步都是**批量处理**，确保高性能！