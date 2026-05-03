#!/usr/bin/env python3
"""Simple test for RAG system with mock embedder."""

import asyncio
import sys
import random
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from agent.rag.types import Document
from agent.rag.storage.chroma_store import ChromaVectorStore


class MockEmbedder:
    """Mock embedder for testing."""

    def __init__(self, dimension=768):
        self.dimension = dimension

    async def embed(self, text: str):
        # Generate deterministic but random-looking embeddings
        random.seed(hash(text) % 1000000)
        return [random.random() for _ in range(self.dimension)]


async def test_chromadb_store():
    """Test ChromaDB vector store."""
    print("=" * 60)
    print("ChromaDB Vector Store Test")
    print("=" * 60)

    # Create mock embedder
    embedder = MockEmbedder(dimension=768)

    # Create vector store
    print("\n📦 Creating ChromaDB vector store...")
    vector_store = ChromaVectorStore(
        path="./data/chroma_test",
        collection_name="test_collection",
        embedder=embedder,
    )
    print("✅ Vector store created")

    # Create test documents
    print("\n📝 Creating test documents...")
    test_docs = [
        Document.create(
            content="Python is a high-level programming language known for its simplicity and readability.",
            source="doc1",
            doc_type="text",
        ),
        Document.create(
            content="Asyncio is a library for writing concurrent code in Python using async/await syntax.",
            source="doc2",
            doc_type="text",
        ),
        Document.create(
            content="ChromaDB is an open-source vector database for AI applications.",
            source="doc3",
            doc_type="text",
        ),
        Document.create(
            content="Ollama allows you to run large language models locally on your machine.",
            source="doc4",
            doc_type="text",
        ),
        Document.create(
            content="向量数据库是用于存储和检索高维向量数据的专门数据库。",
            source="doc5",
            doc_type="text",
        ),
    ]
    print(f"   Created {len(test_docs)} test documents")

    # Add documents
    print("\n💾 Adding documents to vector store...")
    await vector_store.add_documents(test_docs)
    print(f"   ✅ Added {vector_store.count()} documents")

    # Test search
    print("\n🔍 Testing similarity search...")

    test_queries = [
        "Python programming language",
        "向量数据库",
        "Ollama",
    ]

    for query in test_queries:
        print(f"\n   Query: '{query}'")
        print("   " + "-" * 50)

        results = await vector_store.search(query, top_k=3)

        if results:
            for i, result in enumerate(results, 1):
                print(f"   [{i}] Score: {result.score:.4f}")
                print(f"       Content: {result.document.content[:70]}...")
        else:
            print("   No results found")

    # Get stats
    print("\n📊 Statistics:")
    info = vector_store.get_collection_info()
    print(f"   Collection: {info['name']}")
    print(f"   Document count: {info['count']}")

    # Clean up
    print("\n🧹 Cleaning up...")
    vector_store.clear()
    print("   ✅ Collection cleared")

    print("\n" + "=" * 60)
    print("✅ Test completed successfully!")
    print("=" * 60)


if __name__ == "__main__":
    asyncio.run(test_chromadb_store())
