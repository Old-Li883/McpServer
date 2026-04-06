"""Base embedder interface."""

from abc import ABC, abstractmethod
from typing import List


class BaseEmbedder(ABC):
    """Abstract base class for text embedders."""

    @abstractmethod
    def embed(self, text: str) -> List[float]:
        """Generate embedding for a single text.

        Args:
            text: The text to embed

        Returns:
            Embedding vector as a list of floats
        """
        pass

    @abstractmethod
    def embed_batch(self, texts: List[str]) -> List[List[float]]:
        """Generate embeddings for multiple texts.

        Args:
            texts: List of texts to embed

        Returns:
            List of embedding vectors
        """
        pass

    @abstractmethod
    @property
    def dimension(self) -> int:
        """Get the dimension of the embedding vectors."""
        pass
