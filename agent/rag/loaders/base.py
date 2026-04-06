"""Base loader interface for document loading."""

from abc import ABC, abstractmethod
from typing import AsyncIterator, Iterator, Optional
from pathlib import Path

from agent.rag.types import Document


class BaseLoader(ABC):
    """Abstract base class for document loaders.

    All loaders should inherit from this class and implement
    the load() and supports() methods.
    """

    @abstractmethod
    def load(self, source: str, **kwargs) -> Iterator[Document]:
        """Load documents from the given source.

        Args:
            source: The source to load from (file path, URL, etc.)
            **kwargs: Additional loader-specific parameters

        Yields:
            Document instances

        Raises:
            LoaderError: If loading fails
        """
        pass

    async def load_async(self, source: str, **kwargs) -> AsyncIterator[Document]:
        """Async version of load().

        Default implementation delegates to sync load().
        Override for proper async support.
        """
        for doc in self.load(source, **kwargs):
            yield doc

    @abstractmethod
    def supports(self, source: str) -> bool:
        """Check if this loader supports the given source.

        Args:
            source: The source to check

        Returns:
            True if this loader can handle the source
        """
        pass

    def validate_source(self, source: str) -> bool:
        """Validate that the source is accessible.

        Args:
            source: The source to validate

        Returns:
            True if source is valid and accessible
        """
        # Default implementation just checks support
        return self.supports(source)


class LoaderError(Exception):
    """Exception raised when document loading fails."""

    def __init__(self, message: str, source: Optional[str] = None):
        self.message = message
        self.source = source
        super().__init__(f"Loader error for '{source}': {message}")


class LoaderRegistry:
    """Registry for document loaders.

    Manages multiple loaders and routes sources to the appropriate loader.
    """

    def __init__(self):
        self._loaders: list[BaseLoader] = []

    def register(self, loader: BaseLoader) -> None:
        """Register a new loader."""
        self._loaders.append(loader)

    def get_loader(self, source: str) -> Optional[BaseLoader]:
        """Get the appropriate loader for the given source."""
        for loader in self._loaders:
            if loader.supports(source):
                return loader
        return None

    def load(self, source: str, **kwargs) -> Iterator[Document]:
        """Load documents using the appropriate loader.

        Raises:
            LoaderError: If no loader supports the source
        """
        loader = self.get_loader(source)
        if loader is None:
            raise LoaderError(f"No loader found for source: {source}", source)
        yield from loader.load(source, **kwargs)

    async def load_async(self, source: str, **kwargs) -> AsyncIterator[Document]:
        """Async version of load()."""
        loader = self.get_loader(source)
        if loader is None:
            raise LoaderError(f"No loader found for source: {source}", source)
        async for doc in loader.load_async(source, **kwargs):
            yield doc

    @property
    def loaders(self) -> list[BaseLoader]:
        """Get all registered loaders."""
        return self._loaders.copy()
