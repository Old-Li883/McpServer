"""Memory classifier.

Automatically classifies conversation content into different
memory types for better organization and retrieval.
"""

import hashlib
import re
from datetime import datetime
from typing import List

from agent.memory.types import Memory, MemoryType, MemoryImportance


class MemoryClassifier:
    """Memory classifier.

    Automatically classifies conversation content into:
    - USER_PREFERENCE: User preferences and settings
    - TASK_CONTEXT: Task-related context
    - KNOWLEDGE: Accumulated knowledge
    - CONVERSATION: General conversation history
    """

    # User preference patterns
    PREFERENCE_PATTERNS = [
        r"\bi (prefer|like|want|need|usually|always|love|hate)\b",
        r"\bmy (preference|choice|favorite|setting)\b",
        r"(我|我喜欢|想要|需要|通常|总是|偏好)",
    ]

    # Task context patterns
    TASK_CONTEXT_PATTERNS = [
        r"\b(working on|task|project|goal|objective|doing|implementing)\b",
        r"(正在|任务|项目|目标|做|实现)",
    ]

    def __init__(self):
        """Initialize the classifier with compiled patterns."""
        self.preference_patterns = [
            re.compile(p, re.IGNORECASE) for p in self.PREFERENCE_PATTERNS
        ]
        self.task_patterns = [
            re.compile(p, re.IGNORECASE) for p in self.TASK_CONTEXT_PATTERNS
        ]

    def classify(self, content: str, role: str) -> MemoryType:
        """Classify memory type based on content and role.

        Args:
            content: Message content
            role: Message role

        Returns:
            Classified memory type
        """
        # User messages are more likely to be preferences or task context
        if role == "user":
            # Check for preferences
            if any(p.search(content) for p in self.preference_patterns):
                return MemoryType.USER_PREFERENCE

            # Check for task context
            if any(p.search(content) for p in self.task_patterns):
                return MemoryType.TASK_CONTEXT

        # Assistant messages are usually knowledge/conversation
        if role == "assistant":
            return MemoryType.CONVERSATION

        # Default to conversation memory
        return MemoryType.CONVERSATION

    async def classify_and_create(
        self,
        content: str,
        role: str,
        importance_score: float,
        metadata: dict = None,
    ) -> Memory:
        """Classify and create a Memory object.

        Args:
            content: Message content
            role: Message role
            importance_score: Calculated importance score
            metadata: Optional additional metadata

        Returns:
            Created Memory object
        """
        memory_type = self.classify(content, role)

        # Determine importance level based on score
        if importance_score >= 0.8:
            importance = MemoryImportance.CRITICAL
        elif importance_score >= 0.6:
            importance = MemoryImportance.IMPORTANT
        elif importance_score >= 0.4:
            importance = MemoryImportance.NORMAL
        else:
            importance = MemoryImportance.TRIVIAL

        # Generate ID
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        memory_id = f"{memory_type.value}_{content_hash}"

        return Memory(
            id=memory_id,
            content=content,
            memory_type=memory_type,
            importance=importance,
            timestamp=datetime.now(),
            metadata=metadata or {},
        )
