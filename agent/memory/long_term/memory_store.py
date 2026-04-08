"""Long-term memory store.

Provides persistent storage for memories using ChromaDB
vector storage and semantic search capabilities.
"""

from datetime import datetime
from pathlib import Path
from typing import List, Optional, Tuple

from agent.memory.types import Memory, MemoryType, MemoryImportance
from agent.rag.storage.chroma_store import ChromaVectorStore
from agent.rag.embeddings.ollama_embedder import OllamaEmbedder
from agent.rag.types import Document


class LongTermMemoryStore:
    """Long-term memory store.

    Reuses RAG's ChromaDB and OllamaEmbedder for vector storage
    and semantic search of memories.
    """

    def __init__(
        self,
        vector_db_path: str = "./data/memory_chroma",
        collection_name: str = "long_term_memory",
        embedder_model: str = "nomic-embed-text",
        embedder_base_url: str = "http://localhost:11434",
    ):
        """Initialize the long-term memory store.

        Args:
            vector_db_path: Path to ChromaDB storage
            collection_name: Name of the collection
            embedder_model: Embedding model name
            embedder_base_url: Ollama base URL
        """
        self.vector_db_path = vector_db_path
        self.collection_name = collection_name

        # Initialize embedder (reuse RAG's)
        self.embedder = OllamaEmbedder(
            model=embedder_model,
            base_url=embedder_base_url,
            cache_path="./data/embeddings_cache",
            enable_cache=True,
        )

        # Initialize vector store (reuse RAG's)
        self.vector_store = ChromaVectorStore(
            path=vector_db_path,
            collection_name=collection_name,
            embedder=self.embedder,
        )

    async def add_memory(self, memory: Memory) -> None:
        """Add a memory to long-term storage.

        Args:
            memory: Memory to add
        """
        # Generate embedding if not present
        if not memory.embedding:
            memory.embedding = await self.embedder.embed(memory.content)

        # Convert to Document format
        doc = Document(
            id=memory.id,
            content=memory.content,
            metadata={
                "memory_type": memory.memory_type.value,
                "importance": memory.importance.value,
                "timestamp": memory.timestamp.isoformat(),
                "access_count": memory.access_count,
                **memory.metadata,
            },
            embedding=memory.embedding,
        )

        # Add to vector store
        await self.vector_store.add_documents([doc])

    async def add_memories(self, memories: List[Memory]) -> None:
        """Add multiple memories to storage.

        Args:
            memories: List of memories to add
        """
        for memory in memories:
            await self.add_memory(memory)

    async def search_memories(
        self,
        query: str,
        top_k: int = 5,
        memory_type: Optional[MemoryType] = None,
    ) -> List[Tuple[Memory, float]]:
        """Search for relevant memories.

        Args:
            query: Search query
            top_k: Number of results to return
            memory_type: Optional memory type filter

        Returns:
            List of (Memory, score) tuples
        """
        # Build filter conditions
        filter_dict = None
        if memory_type:
            filter_dict = {"memory_type": memory_type.value}

        # Search
        search_results = await self.vector_store.search(
            query=query,
            top_k=top_k,
            filter=filter_dict,
        )

        # Convert to Memory objects
        memories = []
        for result in search_results:
            memory = Memory(
                id=result.document.id,
                content=result.document.content,
                memory_type=MemoryType(
                    result.document.metadata.get("memory_type", "knowledge")
                ),
                importance=MemoryImportance(
                    result.document.metadata.get("importance", "normal")
                ),
                timestamp=datetime.fromisoformat(
                    result.document.metadata.get("timestamp", datetime.now().isoformat())
                ),
                embedding=result.document.embedding,
                metadata={
                    k: v for k, v in result.document.metadata.items()
                    if k not in ["memory_type", "importance", "timestamp"]
                },
                access_count=result.document.metadata.get("access_count", 0),
            )
            memories.append((memory, result.score))

        return memories

    async def delete_memory(self, memory_id: str) -> None:
        """Delete a memory by ID.

        Args:
            memory_id: ID of memory to delete
        """
        self.vector_store.delete(ids=[memory_id])

    def clear_all_memories(self) -> None:
        """Clear all memories (dangerous operation)."""
        self.vector_store.clear()

    def get_stats(self) -> dict:
        """Get storage statistics.

        Returns:
            Dictionary with storage info
        """
        return self.vector_store.get_collection_info()
