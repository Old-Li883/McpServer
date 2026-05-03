"""Memory orchestrator.

Integrates short-term and long-term memory, providing
a unified interface for memory operations.
"""

from datetime import datetime
from pathlib import Path
from typing import List, Optional

from agent.memory.types import Memory, MemoryQueryResult, MemoryType, MemoryImportance
from agent.memory.short_term.memory_manager import ShortTermMemoryManager
from agent.memory.long_term.memory_store import LongTermMemoryStore
from agent.memory.long_term.memory_classifier import MemoryClassifier


class MemoryOrchestrator:
    """Memory orchestrator.

    Integrates short-term and long-term memory systems,
    providing a unified interface for memory operations.
    """

    def __init__(
        self,
        short_term_max_messages: int = 100,
        long_term_vector_db_path: str = "./data/memory_chroma",
        long_term_embedder_model: str = "nomic-embed-text",
        auto_save_threshold: float = 0.7,
    ):
        """Initialize the memory orchestrator.

        Args:
            short_term_max_messages: Max messages in short-term memory
            long_term_vector_db_path: Path to long-term storage
            long_term_embedder_model: Embedding model for memories
            auto_save_threshold: Auto-save memories above this score
        """
        # Initialize components
        self.short_term = ShortTermMemoryManager(
            max_messages=short_term_max_messages,
        )
        self.long_term = LongTermMemoryStore(
            vector_db_path=long_term_vector_db_path,
            embedder_model=long_term_embedder_model,
        )
        self.classifier = MemoryClassifier()

        self.auto_save_threshold = auto_save_threshold
        self._initialized = False

    async def initialize(self) -> None:
        """Initialize the memory system."""
        if self._initialized:
            return

        # Ensure vector storage directory exists
        Path(self.long_term.vector_db_path).mkdir(parents=True, exist_ok=True)

        self._initialized = True

    async def add_conversation_message(
        self,
        message_id: str,
        role: str,
        content: str,
    ) -> None:
        """Add a conversation message to short-term memory.

        Args:
            message_id: Unique message identifier
            role: Message role
            content: Message content
        """
        await self.short_term.add_message(message_id, role, content)

    async def save_important_memories(self) -> int:
        """Save important short-term memories to long-term storage.

        Returns:
            Number of memories saved
        """
        # Get memories worth saving
        memories_to_save = self.short_term.get_memories_for_long_term()

        if not memories_to_save:
            return 0

        # Classify and create Memory objects
        long_term_memories = []
        for conv_mem in memories_to_save:
            if conv_mem.importance_score >= self.auto_save_threshold:
                memory = await self.classifier.classify_and_create(
                    content=conv_mem.content,
                    role=conv_mem.role,
                    importance_score=conv_mem.importance_score,
                    metadata={"original_message_id": conv_mem.message_id},
                )
                long_term_memories.append(memory)

        # Batch save
        await self.long_term.add_memories(long_term_memories)

        return len(long_term_memories)

    async def retrieve_relevant_memories(
        self,
        query: str,
        top_k: int = 5,
        memory_types: Optional[List[MemoryType]] = None,
    ) -> MemoryQueryResult:
        """Retrieve relevant memories.

        Args:
            query: Search query
            top_k: Number of results
            memory_types: Optional memory type filters

        Returns:
            Memory query result
        """
        # Search long-term memory
        all_results = []

        types_to_search = memory_types or list(MemoryType)
        for mem_type in types_to_search:
            results = await self.long_term.search_memories(
                query=query,
                top_k=top_k,
                memory_type=mem_type,
            )
            all_results.extend(results)

        # Sort by score
        all_results.sort(key=lambda x: x[1], reverse=True)

        # Take top_k
        top_results = all_results[:top_k]

        memories = [r[0] for r in top_results]
        scores = [r[1] for r in top_results]

        return MemoryQueryResult(
            memories=memories,
            query=query,
            scores=scores,
            total_count=len(all_results),
        )

    def get_conversation_context(self, max_tokens: int = 2000) -> str:
        """Get conversation context for LLM.

        Args:
            max_tokens: Maximum token budget

        Returns:
            Formatted conversation context
        """
        return self.short_term.get_memory_context(max_tokens)

    def format_memories_for_llm(self, memory_result: MemoryQueryResult) -> str:
        """Format memories for LLM consumption.

        Args:
            memory_result: Memory query result

        Returns:
            Formatted memory text
        """
        if not memory_result.memories:
            return "No relevant memories found."

        parts = ["## Relevant Memories\n"]

        for i, (memory, score) in enumerate(
            zip(memory_result.memories, memory_result.scores), 1
        ):
            parts.append(
                f"### Memory {i} (Relevance: {score:.1%}, Type: {memory.memory_type.value})\n"
                f"{memory.content}\n"
            )

        return "\n".join(parts)

    async def create_manual_memory(
        self,
        content: str,
        memory_type: MemoryType,
        importance: str = "important",
    ) -> Memory:
        """Manually create a memory.

        Args:
            content: Memory content
            memory_type: Type of memory
            importance: Importance level

        Returns:
            Created memory
        """
        import hashlib

        memory_id = f"manual_{hashlib.md5(content.encode()).hexdigest()[:8]}"
        memory = Memory(
            id=memory_id,
            content=content,
            memory_type=memory_type,
            importance=MemoryImportance(importance),
            timestamp=datetime.now(),
            metadata={"source": "manual"},
        )

        await self.long_term.add_memory(memory)
        return memory

    async def search_memories(
        self,
        query: str,
        top_k: int = 5,
    ) -> List[Memory]:
        """Search memories (simplified interface).

        Args:
            query: Search query
            top_k: Number of results

        Returns:
            List of memories
        """
        result = await self.retrieve_relevant_memories(query, top_k)
        return result.memories

    def get_stats(self) -> dict:
        """Get memory system statistics.

        Returns:
            Dictionary with statistics
        """
        return {
            "short_term": {
                "total_memories": len(self.short_term.memories),
                "max_messages": self.short_term.max_messages,
            },
            "long_term": self.long_term.get_stats(),
        }

    async def clear_conversation(self) -> None:
        """Clear current conversation's short-term memory."""
        self.short_term.memories.clear()

    def clear_all_memories(self) -> None:
        """Clear all long-term memories (dangerous)."""
        self.long_term.clear_all_memories()
