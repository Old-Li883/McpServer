"""Message importance scorer.

Evaluates the importance of conversation messages using
heuristic rules to determine what should be preserved.
"""

import re
from typing import List


class ImportanceScorer:
    """Message importance scorer.

    Scores messages based on heuristic rules:
    1. Questions have higher weight
    2. Messages with decisions/configs/preferences have higher weight
    3. Messages with errors/problems have higher weight
    4. Polite exchanges have lower weight
    """

    # High weight patterns
    HIGH_IMPORTANCE_PATTERNS = [
        # Questions
        r"\?|？",
        r"\b(how|what|why|when|where|who|which|can|could|should|would)\b",
        r"\b(如何|什么|为什么|怎么|哪个|是否|是否)\b",
        # Decisions
        r"\b(choose|decide|prefer|want|need)\b",
        r"\b(选择|决定|想要|需要|偏好)\b",
        # Config
        r"\b(config|setting|parameter|option)\b",
        r"\b(配置|设置|参数|选项)\b",
        # Problems
        r"\b(error|problem|issue|bug|fail)\b",
        r"\b(错误|问题|失败|bug)\b",
    ]

    # Low weight patterns
    LOW_IMPORTANCE_PATTERNS = [
        r"^\s*(ok|okay|yes|no|thanks|thank you|please)",
        r"^\s*(好的|是的|不是|谢谢|请)",
        r"^\s*(hmm|uh|um)",
    ]

    def __init__(self):
        """Initialize the scorer with compiled patterns."""
        self.high_patterns = [re.compile(p, re.IGNORECASE) for p in self.HIGH_IMPORTANCE_PATTERNS]
        self.low_patterns = [re.compile(p, re.IGNORECASE) for p in self.LOW_IMPORTANCE_PATTERNS]

    async def score(self, content: str, role: str) -> float:
        """Calculate message importance score (0.0-1.0).

        Args:
            content: Message content
            role: Message role (user/assistant/system)

        Returns:
            Importance score between 0.0 and 1.0
        """
        score = 0.5  # Base score

        # Role weight
        if role == "user":
            score += 0.2
        elif role == "system":
            score += 0.1

        # Content length weight (too short or too long reduces weight)
        length = len(content)
        if 10 < length < 500:
            score += 0.1
        elif length < 5 or length > 1000:
            score -= 0.1

        # Keyword matching
        high_matches = sum(1 for p in self.high_patterns if p.search(content))
        low_matches = sum(1 for p in self.low_patterns if p.search(content))

        score += min(high_matches * 0.15, 0.3)  # Max +0.3
        score -= min(low_matches * 0.2, 0.3)  # Max -0.3

        # Code/commands have higher weight
        code_indicators = ["```", "function", "class", "def ", "import", "const ", "let ", "var "]
        if any(indicator in content for indicator in code_indicators):
            score += 0.2

        return max(0.0, min(1.0, score))
