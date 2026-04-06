"""Chinese text optimization for RAG.

This module provides Chinese-specific text processing including
word segmentation and mixed Chinese-English handling.
"""

import re
from typing import List, Optional

# Try to import jieba
try:
    import jieba
    import jieba.posseg as pseg
    JIEBA_AVAILABLE = True
except ImportError:
    JIEBA_AVAILABLE = False


class ChineseTextProcessor:
    """Processor for Chinese text optimization."""

    # Chinese punctuation marks
    CHINESE_PUNCTUATION = r'[，。！？；：""''（）【】《》、]'

    # Chinese sentence patterns
    CHINESE_SENTENCE_ENDINGS = r'[。！？；]'

    def __init__(
        self,
        enable_segmentation: bool = True,
        preserve_phrases: bool = True,
        mixed_language_handling: bool = True,
    ):
        """Initialize the Chinese text processor.

        Args:
            enable_segmentation: Enable word segmentation (requires jieba)
            preserve_phrases: Preserve meaningful phrases
            mixed_language_handling: Handle Chinese-English mixed content
        """
        self.enable_segmentation = enable_segmentation and JIEBA_AVAILABLE
        self.preserve_phrases = preserve_phrases
        self.mixed_language_handling = mixed_language_handling

        if not JIEBA_AVAILABLE and enable_segmentation:
            print("Warning: jieba not installed. Chinese segmentation disabled.")
            print("Install with: pip install jieba")

    def is_chinese(self, text: str) -> bool:
        """Check if text is primarily Chinese.

        Args:
            text: Input text

        Returns:
            True if text is >50% Chinese characters
        """
        chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', text))
        total_chars = len(text.strip())
        return total_chars > 0 and (chinese_chars / total_chars) > 0.5

    def segment(self, text: str) -> List[str]:
        """Segment Chinese text into words.

        Args:
            text: Input text

        Returns:
            List of words/tokens
        """
        if not self.enable_segmentation:
            # Fall back to character-based segmentation
            return self._character_segment(text)

        # Use jieba for word segmentation
        words = []
        for word, flag in pseg.cut(text):
            # Filter out punctuation and whitespace
            if not word.strip():
                continue
            if re.match(r'^[^\w\u4e00-\u9fff]+$', word):
                continue

            # Preserve meaningful phrases
            if self.preserve_phrases:
                words.append(word)
            else:
                # Split into characters
                words.extend(list(word))

        return words

    def _character_segment(self, text: str) -> List[str]:
        """Character-based segmentation fallback.

        Splits Chinese text into characters while keeping
        English words together.
        """
        tokens = []
        current_english = []

        for char in text:
            if re.match(r'[\u4e00-\u9fff]', char):
                # Chinese character
                if current_english:
                    tokens.append(''.join(current_english))
                    current_english = []
                tokens.append(char)
            elif re.match(r'\w', char):
                # English/number character
                current_english.append(char)
            elif char.strip():
                # Other character (punctuation, etc.)
                if current_english:
                    tokens.append(''.join(current_english))
                    current_english = []
                # Skip punctuation

        if current_english:
            tokens.append(''.join(current_english))

        return tokens

    def split_sentences(self, text: str) -> List[str]:
        """Split Chinese text into sentences.

        Args:
            text: Input text

        Returns:
            List of sentences
        """
        # Split by Chinese sentence endings
        sentences = re.split(self.CHINESE_SENTENCE_ENDINGS, text)

        # Clean up sentences
        sentences = [s.strip() for s in sentences if s.strip()]

        return sentences

    def normalize_punctuation(self, text: str) -> str:
        """Normalize Chinese punctuation.

        Converts between full-width and half-width punctuation
        for consistency.

        Args:
            text: Input text

        Returns:
            Text with normalized punctuation
        """
        # Full-width to half-width (optional, for storage efficiency)
        # text = text.replace('。', '.').replace('，', ',')

        # Half-width to full-width (more standard for Chinese)
        text = text.replace('.', '。').replace(',', '，')
        text = text.replace('!', '！').replace('?', '？')
        text = text.replace(':', '：').replace(';', '；')

        return text

    def extract_keywords(self, text: str, top_k: int = 10) -> List[str]:
        """Extract important keywords from Chinese text.

        Args:
            text: Input text
            top_k: Number of top keywords to return

        Returns:
            List of keywords
        """
        if not self.enable_segmentation:
            # Fall back to character frequency
            return self._extract_char_keywords(text, top_k)

        # Use jieba's TF-IDF extractor if available
        try:
            import jieba.analyse
            keywords = jieba.analyse.extract_tags(text, topK=top_k)
            return keywords
        except (ImportError, AttributeError):
            # Fall back to word frequency
            word_freq = {}
            for word in self.segment(text):
                if len(word) > 1:  # Ignore single characters
                    word_freq[word] = word_freq.get(word, 0) + 1

            # Sort by frequency
            sorted_words = sorted(word_freq.items(), key=lambda x: x[1], reverse=True)
            return [word for word, _ in sorted_words[:top_k]]

    def _extract_char_keywords(self, text: str, top_k: int) -> List[str]:
        """Extract keywords based on character frequency."""
        char_freq = {}
        for char in text:
            if re.match(r'[\u4e00-\u9fff]', char):
                char_freq[char] = char_freq.get(char, 0) + 1

        sorted_chars = sorted(char_freq.items(), key=lambda x: x[1], reverse=True)
        return [char for char, _ in sorted_chars[:top_k]]

    def handle_mixed_language(self, text: str) -> dict:
        """Analyze and handle mixed Chinese-English text.

        Args:
            text: Input text

        Returns:
            Dictionary with language statistics
        """
        # Count different character types
        chinese_chars = len(re.findall(r'[\u4e00-\u9fff]', text))
        english_chars = len(re.findall(r'[a-zA-Z]', text))
        numbers = len(re.findall(r'\d', text))
        total_chars = len(text.replace(' ', ''))

        if total_chars == 0:
            return {
                "chinese_ratio": 0,
                "english_ratio": 0,
                "number_ratio": 0,
                "dominant": "unknown"
            }

        stats = {
            "chinese_ratio": chinese_chars / total_chars,
            "english_ratio": english_chars / total_chars,
            "number_ratio": numbers / total_chars,
            "total_chars": total_chars,
        }

        # Determine dominant language
        if stats["chinese_ratio"] > 0.5:
            stats["dominant"] = "chinese"
        elif stats["english_ratio"] > 0.5:
            stats["dominant"] = "english"
        else:
            stats["dominant"] = "mixed"

        return stats

    def optimize_for_search(self, text: str) -> str:
        """Optimize Chinese text for vector search.

        This prepares text for better embedding by:
        1. Normalizing punctuation
        2. Preserving important phrases
        3. Handling mixed language content

        Args:
            text: Input text

        Returns:
            Optimized text
        """
        # Analyze language mix
        if self.mixed_language_handling:
            stats = self.handle_mixed_language(text)

            # For mixed content, ensure proper spacing
            if stats["dominant"] == "mixed":
                text = self._add_spacing_mixed(text)

        # Normalize punctuation
        text = self.normalize_punctuation(text)

        return text

    def _add_spacing_mixed(self, text: str) -> str:
        """Add proper spacing in mixed Chinese-English text.

        Ensures there are spaces between Chinese and English words.
        """
        # Add space between Chinese and English
        text = re.sub(r'([\u4e00-\u9fff])([a-zA-Z0-9])', r'\1 \2', text)
        text = re.sub(r'([a-zA-Z0-9])([\u4e00-\u9fff])', r'\1 \2', text)

        return text


class ChineseSentenceSplitter:
    """Split Chinese text into sentences while preserving meaning."""

    def __init__(self, min_length: int = 10):
        """Initialize the sentence splitter.

        Args:
            min_length: Minimum sentence length
        """
        self.min_length = min_length

    def split(self, text: str) -> List[str]:
        """Split text into sentences.

        Args:
            text: Input text

        Returns:
            List of sentences
        """
        # Split by major delimiters first
        major_splits = re.split(r'[。！？]', text)

        sentences = []
        for split in major_splits:
            split = split.strip()
            if not split:
                continue

            # Check for minor delimiters within the split
            minor_splits = re.split(r'[；,，]', split)

            for minor_split in minor_splits:
                minor_split = minor_split.strip()
                if len(minor_split) >= self.min_length:
                    sentences.append(minor_split)

        return sentences
