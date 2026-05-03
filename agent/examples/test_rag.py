#!/usr/bin/env python3
"""Simple test script for the RAG system."""

import asyncio
import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from agent.rag import RAGEngine, RAGConfig, Document


async def test_rag_system():
    """Test the RAG system with sample documents."""
    print("=" * 60)
    print("RAG System Test")
    print("=" * 60)

    # Create config
    config = RAGConfig(
        embedder_model="nomic-embed-text",
        vector_db_path="./data/chroma_test",
        collection_name="test_collection",
        top_k=3,
        enable_cache=True,
    )

    print("\n📦 Initializing RAG Engine...")
    async with RAGEngine(config) as engine:
        print("✅ RAG Engine initialized")

        # Create some test documents
        print("\n📝 Creating test documents...")
        test_docs = [
            Document.create(
                content="Python is a high-level programming language known for its simplicity and readability.",
                source="test1",
                doc_type="text",
            ),
            Document.create(
                content="Asyncio is a library for writing concurrent code in Python using async/await syntax.",
                source="test2",
                doc_type="text",
            ),
            Document.create(
                content="ChromaDB is an open-source vector database for AI applications.",
                source="test3",
                doc_type="text",
            ),
            Document.create(
                content="Ollama allows you to run large language models locally on your machine.",
                source="test4",
                doc_type="text",
            ),
            Document.create(
                content="向量数据库是用于存储和检索高维向量数据的专门数据库。",
                source="test5",
                doc_type="text",
            ),
        ]

        print(f"   Created {len(test_docs)} test documents")

        # Add documents to vector store
        print("\n💾 Adding documents to vector store...")
        from agent.rag.storage.chroma_store import ChromaVectorStore

        vector_store = ChromaVectorStore(
            path=config.vector_db_path,
            collection_name=config.collection_name,
            embedder=engine._embedder,
        )
        await vector_store.add_documents(test_docs)
        print(f"   ✅ Added {vector_store.count()} documents")

        # Test queries
        print("\n🔍 Testing queries...")

        queries = [
            "What is Python?",
            "How does async work?",
            "Tell me about vector databases",
            "什么是向量数据库？",  # Chinese query
        ]

        for query in queries:
            print(f"\n   Query: {query}")
            print("   " + "-" * 50)

            result = await engine.query(query)

            print(f"   Found {result.source_count} sources")

            if result.has_sources:
                for i, source in enumerate(result.sources[:2], 1):
                    print(f"   [{i}] Score: {source.score:.3f}")
                    print(f"       Content: {source.document.content[:80]}...")

        # Get stats
        print("\n📊 Engine Statistics:")
        stats = engine.get_stats()
        print(f"   Collection: {stats['collection_info']['name']}")
        print(f"   Document count: {stats['collection_info']['count']}")
        print(f"   Embeddings cached: Yes" if config.enable_cache else "No")

    print("\n" + "=" * 60)
    print("✅ All tests completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    asyncio.run(test_rag_system())
