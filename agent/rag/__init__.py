"""RAG (Retrieval-Augmented Generation) module for the MCP Agent.

This module provides a complete RAG implementation with support for:
- Multiple document sources (local files, web pages, code repositories)
- ChromaDB vector storage
- Ollama local embeddings
- Intelligent retrieval and reranking
- Conversation history and feedback learning
"""

from agent.rag.types import Document, QueryResult, RAGConfig
from agent.rag.core.rag_engine import RAGEngine

__all__ = [
    "Document",
    "QueryResult",
    "RAGConfig",
    "RAGEngine",
]
