"""Document chunking strategies for RAG.

This module provides various chunking strategies to split large documents
into smaller, more manageable pieces for vector storage and retrieval.
"""

import re
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import List, Optional, Iterator
from pathlib import Path

from agent.rag.types import Document


@dataclass
class Chunk:
    """A document chunk.

    Attributes:
        content: The chunk content
        doc_id: Original document ID
        chunk_id: Unique chunk identifier
        metadata: Additional metadata
        token_count: Approximate token count
    """

    content: str
    doc_id: str
    chunk_id: str
    metadata: dict = field(default_factory=dict)
    token_count: int = 0

    def to_document(self) -> Document:
        """Convert chunk to Document."""
        return Document(
            id=self.chunk_id,
            content=self.content,
            metadata={
                **self.metadata,
                "doc_id": self.doc_id,
                "chunk_id": self.chunk_id,
                "token_count": self.token_count,
            }
        )


class BaseChunker(ABC):
    """Abstract base class for document chunkers."""

    def __init__(
        self,
        chunk_size: int = 512,
        chunk_overlap: int = 50,
    ):
        """Initialize the chunker.

        Args:
            chunk_size: Target size of each chunk in tokens/characters
            chunk_overlap: Overlap between consecutive chunks
        """
        self.chunk_size = chunk_size
        self.chunk_overlap = chunk_overlap

    @abstractmethod
    def chunk(self, document: Document) -> List[Chunk]:
        """Split a document into chunks.

        Args:
            document: The document to chunk

        Returns:
            List of chunks
        """
        pass

    def _estimate_tokens(self, text: str) -> int:
        """Estimate token count for text.

        Rough approximation: 1 token ≈ 4 characters for English,
        1 token ≈ 2 characters for Chinese.
        """
        # Count Chinese characters
        chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', text))
        # Count other characters
        other_chars = len(text) - chinese_chars

        # Approximate token count
        return (chinese_chars // 2) + (other_chars // 4)

    def _generate_chunk_id(self, doc_id: str, index: int) -> str:
        """Generate a unique chunk ID."""
        return f"{doc_id}_chunk_{index:04d}"


class FixedChunker(BaseChunker):
    """Fixed-size chunker.

    Splits documents into fixed-size chunks with overlap.
    """

    def chunk(self, document: Document) -> List[Chunk]:
        """Split document into fixed-size chunks."""
        content = document.content
        doc_id = document.id

        chunks = []
        start = 0
        index = 0

        while start < len(content):
            end = start + self.chunk_size

            # Try to end at a sentence boundary
            if end < len(content):
                # Look for sentence endings
                for delimiter in ['\n\n', '\n', '. ', '。', '! ', '！', '? ', '？']:
                    last_pos = content.rfind(delimiter, start, end)
                    if last_pos > start:
                        end = last_pos + len(delimiter)
                        break

            chunk_content = content[start:end].strip()

            if chunk_content:
                token_count = self._estimate_tokens(chunk_content)
                chunk = Chunk(
                    content=chunk_content,
                    doc_id=doc_id,
                    chunk_id=self._generate_chunk_id(doc_id, index),
                    metadata=document.metadata.copy(),
                    token_count=token_count,
                )
                chunks.append(chunk)
                index += 1

            # Move start position with overlap
            start = end - self.chunk_overlap
            if start < 0:
                start = 0

        return chunks


class SemanticChunker(BaseChunker):
    """Semantic chunker based on sentence boundaries.

    Tries to keep semantically related content together.
    """

    # Sentence delimiters
    SENTENCE_DELIMITERS = [
        r'\n\n+',  # Paragraph breaks
        r'\n',      # Line breaks
        r'[.。]\s+',  # Periods
        r'[!！]\s+',  # Exclamation marks
        r'[?？]\s+',  # Question marks
    ]

    def __init__(
        self,
        chunk_size: int = 512,
        chunk_overlap: int = 50,
        min_chunk_size: int = 50,
    ):
        """Initialize the semantic chunker.

        Args:
            chunk_size: Target size in tokens
            chunk_overlap: Overlap between chunks
            min_chunk_size: Minimum chunk size (avoid tiny chunks)
        """
        super().__init__(chunk_size, chunk_overlap)
        self.min_chunk_size = min_chunk_size

    def chunk(self, document: Document) -> List[Chunk]:
        """Split document into semantic chunks."""
        sentences = self._split_sentences(document.content)
        doc_id = document.id

        chunks = []
        current_chunk = ""
        chunk_start_index = 0
        chunk_index = 0

        for i, sentence in enumerate(sentences):
            sentence = sentence.strip()
            if not sentence:
                continue

            test_chunk = current_chunk + "\n" + sentence if current_chunk else sentence
            test_tokens = self._estimate_tokens(test_chunk)

            if test_tokens <= self.chunk_size:
                current_chunk = test_chunk
            else:
                # Current chunk is full, save it
                if current_chunk:
                    if len(current_chunk) >= self.min_chunk_size:
                        token_count = self._estimate_tokens(current_chunk)
                        chunk = Chunk(
                            content=current_chunk,
                            doc_id=doc_id,
                            chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                            metadata={
                                **document.metadata.copy(),
                                "start_sentence": chunk_start_index,
                                "end_sentence": i - 1,
                            },
                            token_count=token_count,
                        )
                        chunks.append(chunk)
                        chunk_index += 1

                # Start new chunk with overlap
                overlap_sentences = self._get_overlap_sentences(
                    sentences,
                    i,
                    overlap_chunks=2
                )
                current_chunk = "\n".join(overlap_sentences + [sentence])
                chunk_start_index = i - len(overlap_sentences)

        # Don't forget the last chunk
        if current_chunk and len(current_chunk) >= self.min_chunk_size:
            token_count = self._estimate_tokens(current_chunk)
            chunk = Chunk(
                content=current_chunk,
                doc_id=doc_id,
                chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                metadata={
                    **document.metadata.copy(),
                    "start_sentence": chunk_start_index,
                    "end_sentence": len(sentences) - 1,
                },
                token_count=token_count,
            )
            chunks.append(chunk)

        return chunks

    def _split_sentences(self, text: str) -> List[str]:
        """Split text into sentences."""
        # First split by paragraph breaks
        paragraphs = re.split(r'\n\s*\n', text)

        sentences = []
        for para in paragraphs:
            para = para.strip()
            if not para:
                continue

            # Try to split paragraph into sentences
            para_sentences = self._split_paragraph_sentences(para)
            sentences.extend(para_sentences)

        return sentences

    def _split_paragraph_sentences(self, paragraph: str) -> List[str]:
        """Split a paragraph into sentences."""
        # Try each delimiter in order
        for pattern in self.SENTENCE_DELIMITERS:
            parts = re.split(f'({pattern})', paragraph)
            if len(parts) > 1:
                # Reconstruct sentences with delimiters
                sentences = []
                current = ""
                for i, part in enumerate(parts):
                    if re.match(pattern, part):
                        if current:
                            sentences.append(current + part)
                            current = ""
                    else:
                        current += part
                if current:
                    sentences.append(current)
                return sentences

        # No delimiter found, return whole paragraph
        return [paragraph]

    def _get_overlap_sentences(
        self,
        sentences: List[str],
        current_index: int,
        overlap_chunks: int = 2,
    ) -> List[str]:
        """Get sentences for overlap from previous chunks."""
        start = max(0, current_index - overlap_chunks)
        return sentences[start:current_index]


class MarkdownChunker(BaseChunker):
    """Markdown-aware chunker.

    Preserves markdown structure and keeps related content together.
    """

    # Markdown headers
    HEADER_PATTERN = re.compile(r'^(#{1,6})\s+(.+)$', re.MULTILINE)

    # Code block pattern
    CODE_BLOCK_PATTERN = re.compile(r'```(\w*)\n(.*?)\n```', re.DOTALL)

    def __init__(
        self,
        chunk_size: int = 512,
        chunk_overlap: int = 50,
        min_chunk_size: int = 100,
    ):
        """Initialize the markdown chunker.

        Args:
            chunk_size: Target size in tokens
            chunk_overlap: Overlap between chunks
            min_chunk_size: Minimum chunk size
        """
        super().__init__(chunk_size, chunk_overlap)
        self.min_chunk_size = min_chunk_size

    def chunk(self, document: Document) -> List[Chunk]:
        """Split markdown document into structured chunks."""
        content = document.content
        doc_id = document.id

        # Split by headers
        sections = self._split_by_headers(content)

        chunks = []
        current_chunk = ""
        current_header = "Introduction"
        chunk_index = 0

        for section_header, section_content in sections:
            if not section_content.strip():
                continue

            test_chunk = current_chunk + "\n\n" + section_content if current_chunk else section_content
            test_tokens = self._estimate_tokens(test_chunk)

            if test_tokens <= self.chunk_size:
                current_chunk = test_chunk
                current_header = section_header
            else:
                # Save current chunk
                if current_chunk:
                    token_count = self._estimate_tokens(current_chunk)
                    chunk = Chunk(
                        content=current_chunk,
                        doc_id=doc_id,
                        chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                        metadata={
                            **document.metadata.copy(),
                            "header": current_header,
                            "type": "markdown",
                        },
                        token_count=token_count,
                    )
                    chunks.append(chunk)
                    chunk_index += 1

                # Start new chunk
                current_chunk = section_content
                current_header = section_header

        # Last chunk
        if current_chunk:
            token_count = self._estimate_tokens(current_chunk)
            chunk = Chunk(
                content=current_chunk,
                doc_id=doc_id,
                chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                metadata={
                    **document.metadata.copy(),
                    "header": current_header,
                    "type": "markdown",
                },
                token_count=token_count,
            )
            chunks.append(chunk)

        return chunks

    def _split_by_headers(self, content: str) -> List[tuple[str, str]]:
        """Split markdown content by headers.

        Returns:
            List of (header, content) tuples
        """
        lines = content.split('\n')

        sections = []
        current_header = "Introduction"
        current_content = []

        for line in lines:
            header_match = self.HEADER_PATTERN.match(line)
            if header_match:
                # Save previous section
                if current_content:
                    sections.append((current_header, '\n'.join(current_content)))

                # Start new section
                level = len(header_match.group(1))
                header_text = header_match.group(2)
                current_header = f"{'#' * level} {header_text}"
                current_content = [line]
            else:
                current_content.append(line)

        # Don't forget the last section
        if current_content:
            sections.append((current_header, '\n'.join(current_content)))

        return sections


class CodeChunker(BaseChunker):
    """Code-aware chunker for programming code.

    Splits code by functions, classes, or logical blocks.
    """

    # Common patterns for function/class definitions
    FUNCTION_PATTERNS = {
        'python': [
            re.compile(r'^def\s+\w+\s*\([^)]*\)\s*:', re.MULTILINE),
            re.compile(r'^class\s+\w+\s*(\([^)]*\))?\s*:', re.MULTILINE),
        ],
        'javascript': [
            re.compile(r'^function\s+\w+\s*\([^)]*\)\s*{', re.MULTILINE),
            re.compile(r'^const\s+\w+\s*=\s*(?:async\s+)?\([^)]*\)\s*=>', re.MULTILINE),
            re.compile(r'^class\s+\w+\s*{', re.MULTILINE),
        ],
        'java': [
            re.compile(r'^(?:public|private|protected)?\s*(?:static)?\s*\w+\s+\w+\s*\([^)]*\)\s*(?:throws\s+[\w\s]+)?\s*{', re.MULTILINE),
            re.compile(r'^(?:public\s+)?class\s+\w+', re.MULTILINE),
        ],
    }

    def __init__(
        self,
        chunk_size: int = 512,
        chunk_overlap: int = 50,
        language: str = 'python',
    ):
        """Initialize the code chunker.

        Args:
            chunk_size: Target size in tokens
            chunk_overlap: Overlap between chunks
            language: Programming language
        """
        super().__init__(chunk_size, chunk_overlap)
        self.language = language.lower()

    def chunk(self, document: Document) -> List[Chunk]:
        """Split code document into logical chunks."""
        content = document.content
        doc_id = document.id

        # Try language-specific splitting
        if self.language in self.FUNCTION_PATTERNS:
            chunks = self._chunk_by_structure(content, doc_id, document.metadata)
        else:
            # Fall back to fixed chunking
            chunks = self._chunk_by_lines(content, doc_id, document.metadata)

        return chunks

    def _chunk_by_structure(
        self,
        content: str,
        doc_id: str,
        metadata: dict,
    ) -> List[Chunk]:
        """Chunk code by function/class structure."""
        patterns = self.FUNCTION_PATTERNS.get(self.language, [])

        lines = content.split('\n')
        chunks = []
        current_chunk = []
        chunk_start = 0
        chunk_index = 0

        for i, line in enumerate(lines):
            current_chunk.append(line)

            # Check if this line starts a new structure
            is_structure = any(pattern.search(line) for pattern in patterns)

            current_content = '\n'.join(current_chunk)
            current_tokens = self._estimate_tokens(current_content)

            if is_structure and current_tokens > self.chunk_size:
                # Save current chunk
                if len(current_chunk) > 3:  # Minimum lines
                    chunk = Chunk(
                        content=current_content,
                        doc_id=doc_id,
                        chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                        metadata={
                            **metadata.copy(),
                            "type": "code",
                            "language": self.language,
                            "start_line": chunk_start,
                            "end_line": i,
                        },
                        token_count=current_tokens,
                    )
                    chunks.append(chunk)
                    chunk_index += 1

                # Start new chunk with overlap
                overlap_lines = max(0, len(current_chunk) - self.chunk_overlap // 2)
                current_chunk = current_chunk[overlap_lines:]
                chunk_start = i - len(current_chunk)

        # Last chunk
        if current_chunk:
            current_content = '\n'.join(current_chunk)
            token_count = self._estimate_tokens(current_content)
            chunk = Chunk(
                content=current_content,
                doc_id=doc_id,
                chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                metadata={
                    **metadata.copy(),
                    "type": "code",
                    "language": self.language,
                    "start_line": chunk_start,
                    "end_line": len(lines) - 1,
                },
                token_count=token_count,
            )
            chunks.append(chunk)

        return chunks

    def _chunk_by_lines(
        self,
        content: str,
        doc_id: str,
        metadata: dict,
    ) -> List[Chunk]:
        """Chunk code by lines (fallback method)."""
        lines = content.split('\n')
        chunks = []
        chunk_index = 0

        lines_per_chunk = max(20, self.chunk_size // 2)  # Approximate

        for i in range(0, len(lines), lines_per_chunk):
            chunk_lines = lines[i:i + lines_per_chunk]
            chunk_content = '\n'.join(chunk_lines)

            token_count = self._estimate_tokens(chunk_content)
            chunk = Chunk(
                content=chunk_content,
                doc_id=doc_id,
                chunk_id=self._generate_chunk_id(doc_id, chunk_index),
                metadata={
                    **metadata.copy(),
                    "type": "code",
                    "language": self.language,
                    "start_line": i,
                    "end_line": i + len(chunk_lines) - 1,
                },
                token_count=token_count,
            )
            chunks.append(chunk)
            chunk_index += 1

        return chunks


class ChunkerFactory:
    """Factory for creating appropriate chunkers."""

    @staticmethod
    def create(
        strategy: str = "semantic",
        **kwargs
    ) -> BaseChunker:
        """Create a chunker based on strategy.

        Args:
            strategy: Chunking strategy (fixed, semantic, markdown, code)
            **kwargs: Additional arguments for the chunker

        Returns:
            A chunker instance
        """
        strategy = strategy.lower()

        if strategy == "fixed":
            return FixedChunker(**kwargs)
        elif strategy == "semantic":
            return SemanticChunker(**kwargs)
        elif strategy == "markdown":
            return MarkdownChunker(**kwargs)
        elif strategy == "code":
            return CodeChunker(**kwargs)
        else:
            raise ValueError(f"Unknown chunking strategy: {strategy}")

    @staticmethod
    def auto_detect(document: Document, **kwargs) -> BaseChunker:
        """Auto-detect and create appropriate chunker.

        Args:
            document: The document to chunk
            **kwargs: Additional arguments for the chunker

        Returns:
            A chunker instance
        """
        doc_type = document.metadata.get("type", "text")
        source = document.metadata.get("source", "")

        # Detect by type
        if doc_type == "markdown":
            return MarkdownChunker(**kwargs)
        elif doc_type == "code":
            language = ChunkerFactory._detect_language(source)
            return CodeChunker(language=language, **kwargs)
        else:
            return SemanticChunker(**kwargs)

    @staticmethod
    def _detect_language(filename: str) -> str:
        """Detect programming language from filename."""
        ext = Path(filename).suffix.lower()

        language_map = {
            '.py': 'python',
            '.js': 'javascript',
            '.ts': 'javascript',
            '.java': 'java',
            '.cpp': 'cpp',
            '.c': 'c',
            '.go': 'go',
            '.rs': 'rust',
        }

        return language_map.get(ext, 'python')
