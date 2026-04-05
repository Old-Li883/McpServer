"""Conversation management for the MCP Agent."""

from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from typing import Any, Optional


class Role(str, Enum):
    """Message role types."""

    SYSTEM = "system"
    USER = "user"
    ASSISTANT = "assistant"
    TOOL = "tool"


@dataclass
class Message:
    """A message in the conversation."""

    role: Role
    content: str
    timestamp: datetime = field(default_factory=datetime.now)
    tool_calls: Optional[list[dict[str, Any]]] = None
    tool_results: Optional[dict[str, Any]] = None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for LLM API."""
        result: dict[str, Any] = {
            "role": self.role.value,
            "content": self.content,
        }
        if self.tool_calls:
            result["tool_calls"] = self.tool_calls
        return result

    @classmethod
    def user(cls, content: str) -> "Message":
        """Create a user message."""
        return cls(role=Role.USER, content=content)

    @classmethod
    def assistant(cls, content: str, tool_calls: Optional[list[dict[str, Any]]] = None) -> "Message":
        """Create an assistant message."""
        return cls(role=Role.ASSISTANT, content=content, tool_calls=tool_calls)

    @classmethod
    def system(cls, content: str) -> "Message":
        """Create a system message."""
        return cls(role=Role.SYSTEM, content=content)

    @classmethod
    def tool(cls, tool_name: str, content: str) -> "Message":
        """Create a tool result message."""
        return cls(
            role=Role.TOOL,
            content=f"[Tool: {tool_name}] {content}",
            tool_results={"tool_name": tool_name, "result": content},
        )


class Conversation:
    """Manages conversation history and context.

    This class maintains the message history for a conversation session,
    handling message addition, retrieval, and history management.
    """

    def __init__(self, max_history: int = 100):
        """Initialize the conversation.

        Args:
            max_history: Maximum number of messages to keep in history
        """
        self.max_history = max_history
        self.messages: list[Message] = []
        self._system_prompt: Optional[str] = None

    def set_system_prompt(self, prompt: str) -> None:
        """Set the system prompt for this conversation.

        Args:
            prompt: System prompt text
        """
        self._system_prompt = prompt

    def add_message(self, message: Message) -> None:
        """Add a message to the conversation.

        Args:
            message: Message to add
        """
        self.messages.append(message)

        # Trim history if needed (but keep system prompt)
        if len(self.messages) > self.max_history:
            # Find and preserve system messages
            system_messages = [m for m in self.messages if m.role == Role.SYSTEM]
            other_messages = [m for m in self.messages if m.role != Role.SYSTEM]

            # Trim other messages
            trim_count = len(self.messages) - self.max_history
            self.messages = system_messages + other_messages[trim_count:]

    def add_user_message(self, content: str) -> Message:
        """Add a user message.

        Args:
            content: Message content

        Returns:
            The created message
        """
        msg = Message.user(content)
        self.add_message(msg)
        return msg

    def add_assistant_message(
        self,
        content: str,
        tool_calls: Optional[list[dict[str, Any]]] = None,
    ) -> Message:
        """Add an assistant message.

        Args:
            content: Message content
            tool_calls: Optional tool calls made

        Returns:
            The created message
        """
        msg = Message.assistant(content, tool_calls)
        self.add_message(msg)
        return msg

    def add_tool_message(self, tool_name: str, content: str) -> Message:
        """Add a tool result message.

        Args:
            tool_name: Name of the tool
            content: Tool result content

        Returns:
            The created message
        """
        msg = Message.tool(tool_name, content)
        self.add_message(msg)
        return msg

    def get_messages(self, include_system: bool = True) -> list[Message]:
        """Get all messages in the conversation.

        Args:
            include_system: Whether to include system messages

        Returns:
            List of messages
        """
        if include_system:
            return self.messages.copy()
        return [m for m in self.messages if m.role != Role.SYSTEM]

    def get_messages_for_llm(self) -> list[dict[str, Any]]:
        """Get messages formatted for LLM API.

        Returns:
            List of message dicts
        """
        messages = self.get_messages(include_system=True)

        # If no system prompt is set, add a default one
        if self._system_prompt and not any(m.role == Role.SYSTEM for m in messages):
            messages = [Message.system(self._system_prompt)] + messages

        return [m.to_dict() for m in messages]

    def clear(self) -> None:
        """Clear all messages from the conversation."""
        self.messages.clear()

    def get_last_n_messages(self, n: int) -> list[Message]:
        """Get the last n messages.

        Args:
            n: Number of messages to get

        Returns:
            List of the last n messages
        """
        return self.messages[-n:] if n < len(self.messages) else self.messages.copy()

    def get_message_count(self) -> int:
        """Get the total number of messages.

        Returns:
            Message count
        """
        return len(self.messages)

    @property
    def is_empty(self) -> bool:
        """Check if conversation is empty."""
        return len(self.messages) == 0

    def __len__(self) -> int:
        """Get the number of messages."""
        return len(self.messages)

    def __repr__(self) -> str:
        """String representation of the conversation."""
        return f"Conversation(messages={len(self.messages)}, max_history={self.max_history})"
