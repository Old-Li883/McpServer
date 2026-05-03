"""Factory for creating document loaders."""

from agent.rag.loaders.base import LoaderRegistry
from agent.rag.loaders.document_loader import DocumentLoader
# Future imports:
# from agent.rag.loaders.web_crawler import WebCrawler
# from agent.rag.loaders.code_loader import CodeLoader


def create_loader_registry() -> LoaderRegistry:
    """Create a loader registry with all available loaders.

    Returns:
        LoaderRegistry with all loaders registered
    """
    registry = LoaderRegistry()

    # Register available loaders
    registry.register(DocumentLoader())

    # Future loaders:
    # registry.register(WebCrawler())
    # registry.register(CodeLoader())

    return registry


# Global registry instance
_global_registry: LoaderRegistry | None = None


def get_global_registry() -> LoaderRegistry:
    """Get the global loader registry instance.

    Creates one if it doesn't exist.
    """
    global _global_registry
    if _global_registry is None:
        _global_registry = create_loader_registry()
    return _global_registry
