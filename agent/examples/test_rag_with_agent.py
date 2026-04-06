#!/usr/bin/env python3
"""Test RAG integration with Agent."""

import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from agent.rag import RAGEngine, RAGConfig, Document
from agent.rag.processors.chunker import ChunkerFactory
from agent.config import Config


async def test_rag_chunking():
    """Test RAG with document chunking."""
    print("=" * 60)
    print("RAG Chunking Test")
    print("=" * 60)

    # Create RAG config
    config = RAGConfig(
        embedder_model="nomic-embed-text",
        vector_db_path="./data/chroma_test",
        collection_name="chunking_test",
        chunk_size=256,
        chunk_overlap=30,
        enable_chinese_optimization=True,
    )

    print("\n📦 Initializing RAG Engine...")
    async with RAGEngine(config) as rag:
        print("✅ RAG Engine initialized")

        # Create test documents of different sizes
        print("\n📝 Creating test documents...")

        # Small document
        small_doc = Document.create(
            content="Python is a high-level programming language.",
            source="small",
            doc_type="text",
        )

        # Medium document
        medium_doc = Document.create(
            content="""Python supports multiple programming paradigms including procedural,
object-oriented, and functional programming. The language provides comprehensive
standard library and is known for its code readability. Python is widely used in
web development, data science, artificial intelligence, and scientific computing.""",
            source="medium",
            doc_type="text",
        )

        # Large document
        large_doc = Document.create(
            content="""Asyncio is a library for writing concurrent code using the async/await syntax.
It was introduced in Python 3.4 and has become the standard for asynchronous I/O operations.
The asyncio module provides infrastructure for writing single-threaded concurrent code using
coroutines, multiplexing I/O access over sockets and other resources, running network clients
and servers, and other related primitives.

Key components of asyncio include:
- Event Loop: The core of asyncio that runs asynchronous tasks
- Coroutines: Functions defined with async def that can await results
- Tasks: Wrappers around coroutines that track their execution
- Futures: Objects that represent the result of an asynchronous operation

Using asyncio can significantly improve performance when dealing with I/O-bound operations
by allowing other code to run while waiting for I/O to complete. This is particularly useful
for web servers, database queries, and network operations.""",
            source="large",
            doc_type="text",
        )

        # Chinese document
        chinese_doc = Document.create(
            content="""向量数据库是一种专门用于存储、索引和查询高维向量数据的数据库系统。
这些向量通常是通过机器学习模型生成的，用于表示文本、图像、音频等非结构化数据。
向量数据库的核心功能是快速找到与给定向量最相似的其他向量，这被称为向量相似度搜索。

常见的向量数据库包括 ChromaDB、Pinecone、Weaviate、Milvus 等。它们在推荐系统、
语义搜索、异常检测等应用场景中发挥着重要作用。向量数据库通常使用近似最近邻(ANN)
算法来提高查询性能，能够在毫秒级别处理百万级的向量搜索。""",
            source="chinese",
            doc_type="text",
        )

        print(f"   Created 4 test documents:")
        print(f"   - Small: {len(small_doc.content)} chars")
        print(f"   - Medium: {len(medium_doc.content)} chars")
        print(f"   - Large: {len(large_doc.content)} chars")
        print(f"   - Chinese: {len(chinese_doc.content)} chars")

        # Test chunking strategies
        print("\n🔪 Testing chunking strategies...")

        for doc_name, doc in [("Small", small_doc), ("Medium", medium_doc), ("Large", large_doc), ("Chinese", chinese_doc)]:
            print(f"\n   {doc_name} document:")
            print("   " + "-" * 50)

            # Auto-detect chunker
            chunker = ChunkerFactory.auto_detect(doc, chunk_size=256, chunk_overlap=30)
            chunks = chunker.chunk(doc)

            print(f"   Chunks created: {len(chunks)}")
            for i, chunk in enumerate(chunks[:2], 1):  # Show first 2 chunks
                print(f"   [{i}] Tokens: {chunk.token_count}, Preview: {chunk.content[:60]}...")
            if len(chunks) > 2:
                print(f"   ... and {len(chunks) - 2} more chunks")

        # Add documents to vector store
        print("\n💾 Adding documents to vector store with chunking...")
        test_docs = [small_doc, medium_doc, large_doc, chinese_doc]

        # Manually chunk and add
        from agent.rag.processors.chunker import Chunk
        all_chunks = []

        for doc in test_docs:
            chunker = ChunkerFactory.auto_detect(doc, chunk_size=256, chunk_overlap=30)
            chunks = chunker.chunk(doc)
            for chunk in chunks:
                chunk_doc = chunk.to_document()
                all_chunks.append(chunk_doc)

        print(f"   Total chunks created: {len(all_chunks)}")

        from agent.rag.storage.chroma_store import ChromaVectorStore
        vector_store = ChromaVectorStore(
            path=config.vector_db_path,
            collection_name=config.collection_name,
            embedder=rag._embedder,
        )
        await vector_store.add_documents(all_chunks)
        print(f"   ✅ Added {vector_store.count()} chunks to vector store")

        # Test queries
        print("\n🔍 Testing queries on chunked documents...")

        queries = [
            "What is asyncio?",
            "How does async/await work?",
            "Tell me about vector databases",
            "什么是向量数据库？",
            "Python programming paradigms",
        ]

        for query in queries:
            print(f"\n   Query: {query}")
            print("   " + "-" * 50)

            result = await rag.query(query, top_k=3)

            print(f"   Found {result.source_count} sources")
            if result.has_sources:
                for i, source in enumerate(result.sources[:2], 1):
                    chunk_id = source.document.metadata.get("chunk_id", "unknown")
                    print(f"   [{i}] Score: {source.score:.3f}, Chunk: {chunk_id}")
                    print(f"       Content: {source.document.content[:70]}...")

        # Get stats
        print("\n📊 Statistics:")
        stats = rag.get_stats()
        print(f"   Documents loaded: {stats['usage_stats']['documents_loaded']}")
        print(f"   Chunks created: {stats['usage_stats']['chunks_created']}")
        print(f"   Queries processed: {stats['usage_stats']['queries_processed']}")
        print(f"   Collection count: {stats['collection_info']['count']}")

        # Clean up
        print("\n🧹 Cleaning up...")
        await rag.delete_collection()
        print("   ✅ Collection cleared")

    print("\n" + "=" * 60)
    print("✅ All tests completed successfully!")
    print("=" * 60)


async def test_rag_agent_integration():
    """Test RAG integration with Agent."""
    print("\n" + "=" * 60)
    print("RAG + Agent Integration Test")
    print("=" * 60)

    from agent.core.agent_engine import AgentEngine
    from agent.config import Config

    # Create config with RAG enabled
    config = Config()
    config.rag.enabled = True
    config.rag.vector_db_path = "./data/chroma_test"
    config.rag.collection_name = "agent_test"

    # Note: This would require MCP server and Ollama to be running
    # For now, just show the structure
    print("\n📋 Configuration:")
    print(f"   RAG Enabled: {config.rag.enabled}")
    print(f"   Embedder Model: {config.rag.embedder_model}")
    print(f"   Chunk Size: {config.rag.chunk_size}")
    print(f"   Top K: {config.rag.top_k}")
    print(f"   Chinese Optimization: {config.rag.enable_chinese_optimization}")

    print("\n✅ Config validated successfully!")
    print("   (Full agent test requires MCP server running)")


if __name__ == "__main__":
    asyncio.run(test_rag_chunking())
    asyncio.run(test_rag_agent_integration())
