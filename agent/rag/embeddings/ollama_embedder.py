"""Ollama-based embedder for local text embeddings."""

import asyncio
from typing import List, Dict, Optional
import hashlib
import json
from pathlib import Path

import httpx


class OllamaEmbedder:
    """Embedder using Ollama's local embedding models.

    Supports models like:
    - nomic-embed-text (768 dimensions)
    - mxbai-embed-large (1024 dimensions)
    - all-minilm (384 dimensions)
    """

    # Default models and their dimensions
    MODEL_DIMENSIONS = {
        "nomic-embed-text": 768,
        "mxbai-embed-large": 1024,
        "all-minilm": 384,
        "all-minilm-l33-v2": 384,
    }

    def __init__(
        self,
        model: str = "nomic-embed-text",
        base_url: str = "http://localhost:11434",
        cache_path: Optional[str] = None,
        enable_cache: bool = True,
    ):
        """Initialize the Ollama embedder.

        Args:
            model: Name of the Ollama embedding model
            base_url: Base URL of the Ollama API
            cache_path: Path to cache directory
            enable_cache: Whether to enable embedding caching
        """
        self.model = model
        self.base_url = base_url.rstrip("/")
        self.enable_cache = enable_cache
        self._client: Optional[httpx.AsyncClient] = None
        self._dimension: Optional[int] = None

        # Setup cache
        self.cache_path = Path(cache_path) if cache_path else None
        self._cache: Optional[Dict[str, List[float]]] = None

        if self.enable_cache and self.cache_path:
            self._load_cache()

    @property
    def client(self) -> httpx.AsyncClient:
        """Get or create HTTP client."""
        if self._client is None:
            self._client = httpx.AsyncClient(
                base_url=self.base_url,
                timeout=60.0,
            )
        return self._client

    @property
    def dimension(self) -> int:
        """Get the embedding dimension."""
        if self._dimension is None:
            # Try to get from known models
            self._dimension = self.MODEL_DIMENSIONS.get(
                self.model,
                self.MODEL_DIMENSIONS["nomic-embed-text"]  # default
            )
        return self._dimension

    async def embed(self, text: str) -> List[float]:
        """Generate embedding for a single text.

        Args:
            text: The text to embed

        Returns:
            Embedding vector
        """
        # Check cache first
        if self.enable_cache:
            cached = self._get_from_cache(text)
            if cached is not None:
                return cached

        # Generate embedding
        embedding = await self._generate_embedding(text)

        # Store in cache
        if self.enable_cache:
            self._store_in_cache(text, embedding)

        return embedding

    async def embed_batch(self, texts: List[str]) -> List[List[float]]:
        """Generate embeddings for multiple texts.

        Args:
            texts: List of texts to embed

        Returns:
            List of embedding vectors
        """
        embeddings = []
        for text in texts:
            embedding = await self.embed(text)
            embeddings.append(embedding)
        return embeddings

    async def _generate_embedding(self, text: str) -> List[float]:
        """Call Ollama API to generate embedding."""
        response = await self.client.post(
            "/api/embeddings",
            json={
                "model": self.model,
                "prompt": text,
            }
        )
        response.raise_for_status()
        data = response.json()

        if "embedding" not in data:
            raise ValueError(f"Unexpected response from Ollama: {data}")

        return data["embedding"]

    def _get_cache_key(self, text: str) -> str:
        """Generate cache key for text."""
        content = f"{self.model}:{text}"
        return hashlib.sha256(content.encode()).hexdigest()

    def _get_from_cache(self, text: str) -> Optional[List[float]]:
        """Get embedding from cache if available."""
        if not self._cache:
            return None

        key = self._get_cache_key(text)
        return self._cache.get(key)

    def _store_in_cache(self, text: str, embedding: List[float]) -> None:
        """Store embedding in cache."""
        if not self._cache:
            return

        key = self._get_cache_key(text)
        self._cache[key] = embedding

        # Persist cache periodically
        self._save_cache()

    def _load_cache(self) -> None:
        """Load cache from disk."""
        if not self.cache_path:
            return

        cache_file = self.cache_path / "embeddings_cache.json"
        if cache_file.exists():
            try:
                with open(cache_file, "r") as f:
                    self._cache = json.load(f)
            except Exception:
                self._cache = {}
        else:
            self._cache = {}

        # Create cache directory if needed
        self.cache_path.mkdir(parents=True, exist_ok=True)

    def _save_cache(self) -> None:
        """Save cache to disk."""
        if not self._cache or not self.cache_path:
            return

        cache_file = self.cache_path / "embeddings_cache.json"
        try:
            with open(cache_file, "w") as f:
                json.dump(self._cache, f)
        except Exception:
            pass  # Ignore save errors

    def clear_cache(self) -> None:
        """Clear the embedding cache."""
        self._cache = {}
        if self.cache_path:
            cache_file = self.cache_path / "embeddings_cache.json"
            if cache_file.exists():
                cache_file.unlink()

    async def close(self) -> None:
        """Close the HTTP client and save cache."""
        if self._client:
            await self._client.aclose()
            self._client = None

        # Save cache before closing
        if self.enable_cache:
            self._save_cache()

    async def __aenter__(self):
        """Async context manager entry."""
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit."""
        await self.close()


class SyncOllamaEmbedder:
    """Synchronous wrapper for OllamaEmbedder.

    Provides a synchronous interface for easier integration.
    """

    def __init__(self, **kwargs):
        self._async_embedder = OllamaEmbedder(**kwargs)
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    def _run_async(self, coro):
        """Run async coroutine in sync context."""
        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        return loop.run_until_complete(coro)

    def embed(self, text: str) -> List[float]:
        """Generate embedding for a single text."""
        return self._run_async(self._async_embedder.embed(text))

    def embed_batch(self, texts: List[str]) -> List[List[float]]:
        """Generate embeddings for multiple texts."""
        return self._run_async(self._async_embedder.embed_batch(texts))

    @property
    def dimension(self) -> int:
        """Get the embedding dimension."""
        return self._async_embedder.dimension

    def close(self):
        """Close the embedder."""
        self._run_async(self._async_embedder.close())

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()
