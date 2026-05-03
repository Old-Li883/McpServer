"""Text preprocessing for RAG.

This module provides text cleaning and normalization functions.
"""

import re
from typing import List, Optional


class TextPreprocessor:
    """Preprocesses text before chunking and embedding."""

    def __init__(
        self,
        clean_whitespace: bool = True,
        normalize_quotes: bool = True,
        remove_urls: bool = False,
        remove_emails: bool = False,
        min_length: int = 10,
    ):
        """Initialize the preprocessor.

        Args:
            clean_whitespace: Clean up excessive whitespace
            normalize_quotes: Normalize quote characters
            remove_urls: Remove URLs from text
            remove_emails: Remove email addresses
            min_length: Minimum text length to keep
        """
        self.clean_whitespace = clean_whitespace
        self.normalize_quotes = normalize_quotes
        self.remove_urls = remove_urls
        self.remove_emails = remove_emails
        self.min_length = min_length

    def preprocess(self, text: str) -> str:
        """Apply all preprocessing steps.

        Args:
            text: Input text

        Returns:
            Preprocessed text
        """
        if not text:
            return ""

        # Apply transformations in order
        if self.remove_urls:
            text = self._remove_urls(text)

        if self.remove_emails:
            text = self._remove_emails(text)

        if self.normalize_quotes:
            text = self._normalize_quotes(text)

        if self.clean_whitespace:
            text = self._clean_whitespace(text)

        # Filter out short texts
        if len(text.strip()) < self.min_length:
            return ""

        return text.strip()

    def _clean_whitespace(self, text: str) -> str:
        """Clean up excessive whitespace."""
        # Replace multiple spaces with single space
        text = re.sub(r' +', ' ', text)
        # Replace multiple newlines with double newline
        text = re.sub(r'\n{3,}', '\n\n', text)
        # Remove leading/trailing whitespace from lines
        lines = [line.strip() for line in text.split('\n')]
        return '\n'.join(lines)

    def _normalize_quotes(self, text: str) -> str:
        """Normalize quote characters."""
        # Normalize smart quotes
        text = text.replace('"', '"').replace('"', '"')
        text = text.replace(''', "'").replace(''', "'")
        text = text.replace('`', "'").replace('`', "'")
        return text

    def _remove_urls(self, text: str) -> str:
        """Remove URLs from text."""
        # Remove http/https URLs
        text = re.sub(r'https?://\S+', '', text)
        # Remove www URLs
        text = re.sub(r'www\.\S+', '', text)
        return text

    def _remove_emails(self, text: str) -> str:
        """Remove email addresses from text."""
        return re.sub(r'\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b', '', text)


class MarkdownPreprocessor(TextPreprocessor):
    """Specialized preprocessor for Markdown documents."""

    def __init__(self, **kwargs):
        """Initialize markdown preprocessor."""
        super().__init__(**kwargs)
        self.preserve_code_blocks = kwargs.get("preserve_code_blocks", True)
        self.preserve_links = kwargs.get("preserve_links", True)

    def preprocess(self, text: str) -> str:
        """Preprocess markdown text."""
        # Extract code blocks if we want to preserve them
        code_blocks = []
        if self.preserve_code_blocks:
            text, code_blocks = self._extract_code_blocks(text)

        # Apply base preprocessing
        text = super().preprocess(text)

        # Restore code blocks
        if self.preserve_code_blocks and code_blocks:
            text = self._restore_code_blocks(text, code_blocks)

        return text

    def _extract_code_blocks(self, text: str) -> tuple[str, List[str]]:
        """Extract code blocks and replace with placeholders."""
        pattern = re.compile(r'```(\w*)\n(.*?)\n```', re.DOTALL)
        code_blocks = []

        def replace_with_placeholder(match):
            code_blocks.append(match.group(0))
            return f"__CODE_BLOCK_{len(code_blocks) - 1}__"

        text = pattern.sub(replace_with_placeholder, text)
        return text, code_blocks

    def _restore_code_blocks(self, text: str, code_blocks: List[str]) -> str:
        """Restore code blocks from placeholders."""
        for i, code_block in enumerate(code_blocks):
            text = text.replace(f"__CODE_BLOCK_{i}__", code_block)
        return text


class CodePreprocessor(TextPreprocessor):
    """Specialized preprocessor for code documents."""

    def __init__(self, **kwargs):
        """Initialize code preprocessor."""
        super().__init__(**kwargs)
        self.preserve_comments = kwargs.get("preserve_comments", True)
        self.preserve_docstrings = kwargs.get("preserve_docstrings", True)
        self.min_length = kwargs.get("min_length", 20)  # Code can be shorter

    def preprocess(self, text: str) -> str:
        """Preprocess code text."""
        # Remove shebang lines
        lines = text.split('\n')
        if lines and lines[0].startswith('#!'):
            lines = lines[1:]

        text = '\n'.join(lines)

        # Apply base preprocessing
        text = super().preprocess(text)

        return text
