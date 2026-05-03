"""Conversation summarizer.

Generates concise summaries of low-importance messages
to reduce memory usage while preserving context.
"""

from typing import List

from agent.memory.types import ConversationMemory


class Summarizer:
    """Conversation summarizer.

    Merges multiple low-importance messages into brief summaries
    to manage memory usage efficiently.
    """

    async def summarize_messages(self, memories: List[ConversationMemory]) -> str:
        """Generate a summary of the given messages.

        Args:
            memories: List of conversation memories to summarize

        Returns:
            Summary text
        """
        if not memories:
            return ""

        # Extract key information by role
        user_msgs = [m for m in memories if m.role == "user"]
        assistant_msgs = [m for m in memories if m.role == "assistant"]

        summary_parts = []

        if user_msgs:
            # Extract key topics from user messages
            user_contents = " ".join([m.content for m in user_msgs])
            topics = self._extract_key_topics(user_contents)
            if topics:
                summary_parts.append(f"User discussed: {topics}")

        if assistant_msgs:
            # Count assistant responses
            summary_parts.append(f"Assistant provided {len(assistant_msgs)} responses")

        return " | ".join(summary_parts)

    def _extract_key_topics(self, text: str, max_topics: int = 3) -> str:
        """Extract key topics from text.

        Args:
            text: Input text
            max_topics: Maximum number of topics to extract

        Returns:
            Space-separated key topics
        """
        # Simple implementation: extract meaningful words
        words = text.split()

        # Filter stop words
        stopwords = {
            "the", "a", "an", "is", "are", "was", "were", "of", "in", "on",
            "at", "to", "for", "with", "by", "from", "as", "and", "or", "but"
        }
        keywords = [w for w in words if w.lower() not in stopwords and len(w) > 3]

        if not keywords:
            return "various topics"

        return " ".join(keywords[:max_topics])
