"""Core data types for the memory system."""

from dataclasses import dataclass, field
from datetime import datetime
from typing import Any, Optional
from enum import Enum


class MemoryType(str, Enum):
    """Memory type classification."""

    USER_PREFERENCE = "user_preference"  # User preferences and settings
    TASK_CONTEXT = "task_context"  # Task-related context
    KNOWLEDGE = "knowledge"  # Accumulated knowledge
    CONVERSATION = "conversation"  # Conversation history


class MemoryImportance(str, Enum):
    """Memory importance level."""

    CRITICAL = "critical"  # Critical memory
    IMPORTANT = "important"  # Important memory
    NORMAL = "normal"  # Normal memory
    TRIVIAL = "trivial"  # Trivial memory


@dataclass
class Memory:
    """Universal memory unit.

    Represents a single memory with content, type, importance,
    and optional embedding for vector search.
    """

    id: str
    content: str
    memory_type: MemoryType
    importance: MemoryImportance
    timestamp: datetime = field(default_factory=datetime.now)
    embedding: Optional[list[float]] = None
    metadata: dict[str, Any] = field(default_factory=dict)
    access_count: int = 0
    last_accessed: Optional[datetime] = None

    def to_dict(self) -> dict[str, Any]:
        """Serialize to dictionary."""
        return {
            "id": self.id,
            "content": self.content,
            "memory_type": self.memory_type.value,
            "importance": self.importance.value,
            "timestamp": self.timestamp.isoformat(),
            "embedding": self.embedding,
            "metadata": self.metadata,
            "access_count": self.access_count,
            "last_accessed": self.last_accessed.isoformat() if self.last_accessed else None,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Memory":
        """Deserialize from dictionary."""
        return cls(
            id=data["id"],
            content=data["content"],
            memory_type=MemoryType(data["memory_type"]),
            importance=MemoryImportance(data["importance"]),
            timestamp=datetime.fromisoformat(data["timestamp"]),
            embedding=data.get("embedding"),
            metadata=data.get("metadata", {}),
            access_count=data.get("access_count", 0),
            last_accessed=datetime.fromisoformat(data["last_accessed"]) if data.get("last_accessed") else None,
        )


@dataclass
class ConversationMemory:
    """Conversation memory with importance score.

    Represents a single message in the conversation with
    an importance score for memory management decisions.
    """

    message_id: str
    role: str
    content: str
    importance_score: float  # 0.0-1.0
    timestamp: datetime = field(default_factory=datetime.now)
    is_summary: bool = False
    summary_of: Optional[list[str]] = None  # If summary, original message IDs

    def to_dict(self) -> dict[str, Any]:
        """Serialize to dictionary."""
        return {
            "message_id": self.message_id,
            "role": self.role,
            "content": self.content,
            "importance_score": self.importance_score,
            "timestamp": self.timestamp.isoformat(),
            "is_summary": self.is_summary,
            "summary_of": self.summary_of,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ConversationMemory":
        """Deserialize from dictionary."""
        return cls(
            message_id=data["message_id"],
            role=data["role"],
            content=data["content"],
            importance_score=data["importance_score"],
            timestamp=datetime.fromisoformat(data["timestamp"]),
            is_summary=data.get("is_summary", False),
            summary_of=data.get("summary_of"),
        )


@dataclass
class MemoryQueryResult:
    """Memory retrieval result.

    Contains the retrieved memories along with relevance scores.
    """

    memories: list[Memory]
    query: str
    scores: list[float]  # Relevance score for each memory
    total_count: int

    def to_dict(self) -> dict[str, Any]:
        """Serialize to dictionary."""
        return {
            "memories": [m.to_dict() for m in self.memories],
            "query": self.query,
            "scores": self.scores,
            "total_count": self.total_count,
        }
