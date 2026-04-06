"""Core data types for the RAG module."""

from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Optional
import hashlib
import json


@dataclass
class Document:
    """A document chunk in the RAG system.

    Attributes:
        id: Unique identifier for this document/chunk
        content: The text content of the document
        metadata: Additional metadata (source, type, timestamp, etc.)
        embedding: Optional pre-computed embedding vector
        chunk_id: Optional identifier for the chunk (if split from larger doc)
        parent_id: Optional parent document ID (if this is a chunk)
    """

    id: str
    content: str
    metadata: dict[str, Any] = field(default_factory=dict)
    embedding: Optional[list[float]] = None
    chunk_id: Optional[str] = None
    parent_id: Optional[str] = None

    @classmethod
    def create(
        cls,
        content: str,
        source: str = "",
        doc_type: str = "text",
        metadata: Optional[dict[str, Any]] = None,
    ) -> "Document":
        """Create a new document with auto-generated ID.

        Args:
            content: Document content
            source: Source identifier (file path, URL, etc.)
            doc_type: Document type (text, markdown, code, etc.)
            metadata: Additional metadata

        Returns:
            A new Document instance
        """
        # Generate unique ID based on content and source
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        timestamp = datetime.now().strftime("%Y%m%d")
        doc_id = f"{doc_type}_{source}_{timestamp}_{content_hash}"

        # Build metadata
        doc_metadata = {
            "source": source,
            "type": doc_type,
            "created_at": datetime.now().isoformat(),
        }
        if metadata:
            doc_metadata.update(metadata)

        return cls(id=doc_id, content=content, metadata=doc_metadata)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "id": self.id,
            "content": self.content,
            "metadata": self.metadata,
            "embedding": self.embedding,
            "chunk_id": self.chunk_id,
            "parent_id": self.parent_id,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Document":
        """Create from dictionary."""
        return cls(
            id=data["id"],
            content=data["content"],
            metadata=data.get("metadata", {}),
            embedding=data.get("embedding"),
            chunk_id=data.get("chunk_id"),
            parent_id=data.get("parent_id"),
        )


@dataclass
class SearchResult:
    """A single search result from the vector database.

    Attributes:
        document: The matched document
        score: Similarity score (0-1, higher is better)
        metadata: Additional result metadata
    """

    document: Document
    score: float
    metadata: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return {
            "document": self.document.to_dict(),
            "score": self.score,
            "metadata": self.metadata,
        }


@dataclass
class QueryResult:
    """Result from a RAG query.

    Attributes:
        answer: The generated answer from the LLM
        sources: List of source documents used
        query: The original query
        retrieved_docs: Documents retrieved from vector search
        metadata: Additional result metadata
    """

    answer: str
    sources: list[SearchResult]
    query: str
    retrieved_docs: list[Document] = field(default_factory=list)
    metadata: dict[str, Any] = field(default_factory=dict)

    @property
    def source_count(self) -> int:
        """Number of sources used."""
        return len(self.sources)

    @property
    def has_sources(self) -> bool:
        """Whether sources were used."""
        return len(self.sources) > 0

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return {
            "answer": self.answer,
            "sources": [s.to_dict() for s in self.sources],
            "query": self.query,
            "source_count": self.source_count,
            "metadata": self.metadata,
        }


@dataclass
class RAGConfig:
    """Configuration for the RAG system.

    Attributes:
        embedder_model: Name of the Ollama embedding model
        vector_db_path: Path to ChromaDB storage
        chunk_size: Default chunk size for document splitting
        chunk_overlap: Overlap between chunks
        top_k: Number of documents to retrieve
        score_threshold: Minimum similarity score
        enable_cache: Enable embedding caching
    """

    # Embedder settings
    embedder_model: str = "nomic-embed-text"
    embedder_base_url: str = "http://localhost:11434"

    # Vector database settings
    vector_db_path: str = "./data/chroma"
    collection_name: str = "default"

    # Chunking settings
    chunk_size: int = 512
    chunk_overlap: int = 50

    # Retrieval settings
    top_k: int = 5
    score_threshold: float = 0.5

    # Performance settings
    enable_cache: bool = True
    cache_path: str = "./data/embeddings_cache"

    # Feature flags
    enable_reranking: bool = False
    enable_query_expansion: bool = False
    enable_chinese_optimization: bool = True
    always_enabled: bool = False  # Always use RAG for all queries

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return {
            "embedder_model": self.embedder_model,
            "embedder_base_url": self.embedder_base_url,
            "vector_db_path": self.vector_db_path,
            "collection_name": self.collection_name,
            "chunk_size": self.chunk_size,
            "chunk_overlap": self.chunk_overlap,
            "top_k": self.top_k,
            "score_threshold": self.score_threshold,
            "enable_cache": self.enable_cache,
            "cache_path": self.cache_path,
            "enable_reranking": self.enable_reranking,
            "enable_query_expansion": self.enable_query_expansion,
            "enable_chinese_optimization": self.enable_chinese_optimization,
            "always_enabled": self.always_enabled,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "RAGConfig":
        """Create from dictionary."""
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})
