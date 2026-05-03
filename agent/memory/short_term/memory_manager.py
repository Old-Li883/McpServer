"""Short-term memory manager.

Manages in-session conversation memory with automatic
importance scoring and compression.
"""

from typing import List

from agent.memory.types import ConversationMemory
from agent.memory.short_term.importance_scorer import ImportanceScorer
from agent.memory.short_term.summarizer import Summarizer


class ShortTermMemoryManager:
    """Short-term memory manager.

    Features:
    1. Manages in-session conversation memories
    2. Auto-scores message importance
    3. Compresses memories when threshold exceeded
    4. Extracts key information for long-term storage
    """

    def __init__(
        self,
        max_messages: int = 100,
        summary_threshold: int = 80,
        importance_threshold: float = 0.6,
    ):
        """Initialize the short-term memory manager.

        Args:
            max_messages: Maximum number of messages to keep
            summary_threshold: Trigger compression at this count
            importance_threshold: Minimum score for long-term storage
        """
        self.max_messages = max_messages
        self.summary_threshold = summary_threshold
        self.importance_threshold = importance_threshold

        self.memories: List[ConversationMemory] = []
        self.importance_scorer = ImportanceScorer()
        self.summarizer = Summarizer()

    async def add_message(
        self,
        message_id: str,
        role: str,
        content: str,
    ) -> ConversationMemory:
        """Add a message and calculate importance.

        Args:
            message_id: Unique message identifier
            role: Message role (user/assistant/system)
            content: Message content

        Returns:
            Created conversation memory
        """
        # Calculate importance score
        importance_score = await self.importance_scorer.score(content, role)

        memory = ConversationMemory(
            message_id=message_id,
            role=role,
            content=content,
            importance_score=importance_score,
        )

        self.memories.append(memory)

        # Check if compression needed
        if len(self.memories) > self.max_messages:
            await self._compress_memories()

        return memory

    async def _compress_memories(self) -> None:
        """Compress low-importance memories."""
        # Find low-importance memories
        low_importance = [
            m for m in self.memories
            if m.importance_score < self.importance_threshold and not m.is_summary
        ]

        if len(low_importance) < self.summary_threshold:
            return

        # Group consecutive low-importance messages
        groups = self._group_consecutive_memories(low_importance)

        # Summarize each group
        for group in groups:
            if len(group) < 3:  # Too few to summarize
                continue

            summary = await self.summarizer.summarize_messages(group)
            summary_memory = ConversationMemory(
                message_id=f"summary_{int(memory.timestamp.timestamp())}",
                role="system",
                content=summary,
                importance_score=sum(m.importance_score for m in group) / len(group),
                is_summary=True,
                summary_of=[m.message_id for m in group],
            )

            # Replace original memories with summary
            self._replace_memories_with_summary(group, summary_memory)

    def _group_consecutive_memories(
        self,
        memories: List[ConversationMemory],
    ) -> List[List[ConversationMemory]]:
        """Group consecutive memories together.

        Args:
            memories: List of memories to group

        Returns:
            List of memory groups
        """
        if not memories:
            return []

        groups = []
        current_group = [memories[0]]

        for i in range(1, len(memories)):
            # Check if consecutive in the main memory list
            if memories[i].message_id in [m.message_id for m in self.memories]:
                idx = next(
                    j for j, m in enumerate(self.memories)
                    if m.message_id == memories[i].message_id
                )
                prev_idx = next(
                    j for j, m in enumerate(self.memories)
                    if m.message_id == current_group[-1].message_id
                )
                if idx == prev_idx + 1:
                    current_group.append(memories[i])
                else:
                    groups.append(current_group)
                    current_group = [memories[i]]

        if current_group:
            groups.append(current_group)

        return groups

    def _replace_memories_with_summary(
        self,
        original_memories: List[ConversationMemory],
        summary: ConversationMemory,
    ) -> None:
        """Replace original memories with summary.

        Args:
            original_memories: Memories to replace
            summary: Summary memory to insert
        """
        for mem in original_memories:
            if mem in self.memories:
                self.memories.remove(mem)
        self.memories.append(summary)

    def get_important_memories(self, threshold: float = 0.7) -> List[ConversationMemory]:
        """Get memories above importance threshold.

        Args:
            threshold: Minimum importance score

        Returns:
            List of important memories
        """
        return [m for m in self.memories if m.importance_score >= threshold]

    def get_memories_for_long_term(self) -> List[ConversationMemory]:
        """Get memories worth saving to long-term storage.

        Returns:
            List of memories for long-term storage
        """
        return [
            m for m in self.memories
            if m.importance_score >= self.importance_threshold
            and not m.is_summary
            and m.role in ["user", "assistant"]
        ]

    def get_memory_context(self, max_tokens: int = 2000) -> str:
        """Get memory content for LLM context.

        Args:
            max_tokens: Maximum token budget (approximate)

        Returns:
            Formatted memory context
        """
        # Sort by importance
        sorted_memories = sorted(
            self.memories,
            key=lambda m: m.importance_score,
            reverse=True,
        )

        context_parts = []
        current_tokens = 0

        for memory in sorted_memories:
            # Rough token estimation (1 token ≈ 4 chars)
            estimated_tokens = len(memory.content) // 4

            if current_tokens + estimated_tokens > max_tokens:
                break

            context_parts.append(f"[{memory.role}]: {memory.content}")
            current_tokens += estimated_tokens

        return "\n".join(context_parts)
