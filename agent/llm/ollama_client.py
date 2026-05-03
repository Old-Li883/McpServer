"""Ollama LLM client integration."""

from typing import Any, Optional

import httpx


class OllamaClientError(Exception):
    """Base exception for Ollama client errors."""

    pass


class OllamaConnectionError(OllamaClientError):
    """Exception raised when connection to Ollama fails."""

    pass


class OllamaClient:
    """Client for interacting with Ollama API.

    This client communicates with a local Ollama instance to generate
    responses using the specified model.
    """

    def __init__(
        self,
        base_url: str = "http://localhost:11434",
        model: str = "llama3.2",
        timeout: float = 120.0,
    ):
        """Initialize the Ollama client.

        Args:
            base_url: Base URL of the Ollama API
            model: Model name to use for generation
            timeout: Request timeout in seconds
        """
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.timeout = timeout
        self._client: Optional[httpx.AsyncClient] = None

    async def __aenter__(self) -> "OllamaClient":
        """Async context manager entry."""
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """Async context manager exit."""
        await self.close()

    async def connect(self) -> None:
        """Connect to the Ollama server.

        Raises:
            OllamaConnectionError: If connection fails
        """
        if self._client is None:
            self._client = httpx.AsyncClient(timeout=self.timeout)

        try:
            # Check if Ollama is running
            await self._check_connection()
        except httpx.ConnectError as e:
            raise OllamaConnectionError(f"Failed to connect to Ollama at {self.base_url}: {e}") from e
        except Exception as e:
            raise OllamaConnectionError(f"Connection check failed: {e}") from e

    async def close(self) -> None:
        """Close the connection to Ollama."""
        if self._client:
            await self._client.aclose()
            self._client = None

    async def _check_connection(self) -> None:
        """Check if Ollama server is responsive."""
        if self._client is None:
            raise OllamaConnectionError("Client is not connected")

        response = await self._client.get(f"{self.base_url}/api/tags")
        response.raise_for_status()

    async def generate(
        self,
        prompt: str,
        system: Optional[str] = None,
        temperature: float = 0.7,
        max_tokens: int = 2048,
        stream: bool = False,
    ) -> str:
        """Generate a response.

        Args:
            prompt: The user prompt
            system: Optional system prompt
            temperature: Sampling temperature (0.0 - 1.0)
            max_tokens: Maximum tokens to generate
            stream: Whether to stream the response

        Returns:
            The generated response text

        Raises:
            OllamaClientError: If generation fails
        """
        if self._client is None:
            raise OllamaClientError("Client is not connected")

        request_body: dict[str, Any] = {
            "model": self.model,
            "prompt": prompt,
            "stream": stream,
            "options": {
                "temperature": temperature,
                "num_predict": max_tokens,
            },
        }

        if system:
            request_body["system"] = system

        try:
            response = await self._client.post(
                f"{self.base_url}/api/generate",
                json=request_body,
                timeout=self.timeout,
            )
            response.raise_for_status()
        except httpx.HTTPError as e:
            raise OllamaClientError(f"Generation request failed: {e}") from e

        data = response.json()
        return data.get("response", "")

    async def chat(
        self,
        messages: list[dict[str, Any]],
        temperature: float = 0.7,
        max_tokens: int = 2048,
        stream: bool = False,
    ) -> str:
        """Generate a chat response.

        Args:
            messages: List of message dicts with 'role' and 'content'
            temperature: Sampling temperature (0.0 - 1.0)
            max_tokens: Maximum tokens to generate
            stream: Whether to stream the response

        Returns:
            The generated response text

        Raises:
            OllamaClientError: If generation fails
        """
        if self._client is None:
            raise OllamaClientError("Client is not connected")

        request_body: dict[str, Any] = {
            "model": self.model,
            "messages": messages,
            "stream": stream,
            "options": {
                "temperature": temperature,
                "num_predict": max_tokens,
            },
        }

        try:
            response = await self._client.post(
                f"{self.base_url}/api/chat",
                json=request_body,
                timeout=self.timeout,
            )
            response.raise_for_status()
        except httpx.HTTPError as e:
            raise OllamaClientError(f"Chat request failed: {e}") from e

        data = response.json()
        return data.get("message", {}).get("content", "")

    async def list_models(self) -> list[str]:
        """List available models.

        Returns:
            List of model names

        Raises:
            OllamaClientError: If listing fails
        """
        if self._client is None:
            raise OllamaClientError("Client is not connected")

        try:
            response = await self._client.get(f"{self.base_url}/api/tags")
            response.raise_for_status()
        except httpx.HTTPError as e:
            raise OllamaClientError(f"Failed to list models: {e}") from e

        data = response.json()
        models = data.get("models", [])
        return [m.get("name", "") for m in models]

    async def pull_model(self, model: str) -> None:
        """Pull a model from Ollama registry.

        Args:
            model: Model name to pull

        Raises:
            OllamaClientError: If pull fails
        """
        if self._client is None:
            raise OllamaClientError("Client is not connected")

        try:
            response = await self._client.post(
                f"{self.base_url}/api/pull",
                json={"name": model, "stream": False},
                timeout=300.0,  # Pulling can take a while
            )
            response.raise_for_status()
        except httpx.HTTPError as e:
            raise OllamaClientError(f"Failed to pull model {model}: {e}") from e
