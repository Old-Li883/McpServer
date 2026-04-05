"""MCP HTTP JSON-RPC client.

Communicates with the C++ MCP Server over HTTP using JSON-RPC 2.0 protocol.
"""

import asyncio
from typing import Any, Optional

import httpx

from agent.mcp.types import (
    ContentItem,
    InitializeResult,
    Prompt,
    PromptResult,
    Resource,
    ResourceContent,
    ServerCapabilities,
    Tool,
    ToolResult,
)


class McpClientError(Exception):
    """Base exception for MCP client errors."""

    pass


class McpConnectionError(McpClientError):
    """Exception raised when connection to MCP server fails."""

    pass


class McpRpcError(McpClientError):
    """Exception raised when JSON-RPC call fails."""

    def __init__(self, code: int, message: str, data: Optional[Any] = None):
        self.code = code
        self.message = message
        self.data = data
        super().__init__(f"RPC error {code}: {message}")


class McpClient:
    """MCP HTTP JSON-RPC client.

    This client communicates with the C++ MCP Server using JSON-RPC 2.0 over HTTP.
    The server exposes endpoints at '/' and '/rpc' for POST requests.
    """

    def __init__(
        self,
        server_url: str = "http://localhost:8080",
        timeout: float = 30.0,
    ):
        """Initialize the MCP client.

        Args:
            server_url: Base URL of the MCP server
            timeout: Request timeout in seconds
        """
        self.server_url = server_url.rstrip("/")
        self.timeout = timeout
        self._request_id = 0
        self._client: Optional[httpx.AsyncClient] = None
        self._capabilities: Optional[ServerCapabilities] = None
        self._initialized = False

    async def __aenter__(self) -> "McpClient":
        """Async context manager entry."""
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb) -> None:
        """Async context manager exit."""
        await self.close()

    async def connect(self) -> None:
        """Connect to the MCP server and initialize.

        Raises:
            McpConnectionError: If connection fails
        """
        if self._client is None:
            self._client = httpx.AsyncClient(timeout=self.timeout)

        try:
            # Initialize the connection
            await self.initialize()
        except httpx.ConnectError as e:
            raise McpConnectionError(f"Failed to connect to {self.server_url}: {e}") from e
        except Exception as e:
            raise McpConnectionError(f"Initialization failed: {e}") from e

    async def close(self) -> None:
        """Close the connection to the MCP server."""
        if self._client:
            await self._client.aclose()
            self._client = None
        self._initialized = False
        self._capabilities = None

    def _next_request_id(self) -> int:
        """Get the next request ID."""
        self._request_id += 1
        return self._request_id

    async def _call(
        self,
        method: str,
        params: Optional[dict[str, Any]] = None,
    ) -> Any:
        """Make a JSON-RPC call.

        Args:
            method: RPC method name
            params: Method parameters

        Returns:
            The result field from the JSON-RPC response

        Raises:
            McpRpcError: If the RPC call returns an error
            McpConnectionError: If the HTTP request fails
        """
        if self._client is None:
            raise McpConnectionError("Client is not connected")

        request_id = self._next_request_id()
        request_body = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method,
        }
        if params is not None:
            request_body["params"] = params

        try:
            response = await self._client.post(
                f"{self.server_url}/",
                json=request_body,
                headers={"Content-Type": "application/json"},
            )
            response.raise_for_status()
        except httpx.HTTPError as e:
            raise McpConnectionError(f"HTTP request failed: {e}") from e

        data = response.json()

        # Check for RPC error
        if "error" in data:
            error = data["error"]
            raise McpRpcError(
                code=error.get("code", -1),
                message=error.get("message", "Unknown error"),
                data=error.get("data"),
            )

        # Return the result
        return data.get("result")

    async def initialize(self) -> InitializeResult:
        """Initialize the MCP connection.

        Returns:
            InitializeResult with server capabilities

        Raises:
            McpRpcError: If the initialize call fails
        """
        result = await self._call("initialize", {})
        init_result = InitializeResult.from_dict(result)
        self._capabilities = init_result.capabilities
        self._initialized = True
        return init_result

    @property
    def capabilities(self) -> ServerCapabilities:
        """Get server capabilities.

        Returns:
            ServerCapabilities

        Raises:
            McpClientError: If client is not initialized
        """
        if not self._initialized or self._capabilities is None:
            raise McpClientError("Client is not initialized")
        return self._capabilities

    # ========== Tool methods ==========

    async def list_tools(self) -> list[Tool]:
        """List available tools.

        Returns:
            List of Tool definitions

        Raises:
            McpRpcError: If the call fails
        """
        result = await self._call("tools/list", {})
        tools_data = result.get("tools", [])
        return [Tool.from_dict(t) for t in tools_data]

    async def call_tool(
        self,
        name: str,
        arguments: Optional[dict[str, Any]] = None,
    ) -> ToolResult:
        """Call a tool.

        Args:
            name: Tool name
            arguments: Tool arguments

        Returns:
            ToolResult with the tool's output

        Raises:
            McpRpcError: If the call fails
        """
        params: dict[str, Any] = {"name": name}
        if arguments is not None:
            params["arguments"] = arguments

        result = await self._call("tools/call", params)
        return ToolResult.from_dict(result)

    # ========== Resource methods ==========

    async def list_resources(self) -> list[Resource]:
        """List available resources.

        Returns:
            List of Resource definitions

        Raises:
            McpRpcError: If the call fails
        """
        result = await self._call("resources/list", {})
        resources_data = result.get("resources", [])
        return [Resource.from_dict(r) for r in resources_data]

    async def read_resource(self, uri: str) -> ResourceContent:
        """Read a resource.

        Args:
            uri: Resource URI

        Returns:
            ResourceContent with the resource's content

        Raises:
            McpRpcError: If the call fails
        """
        result = await self._call("resources/read", {"uri": uri})
        return ResourceContent.from_dict(result)

    # ========== Prompt methods ==========

    async def list_prompts(self) -> list[Prompt]:
        """List available prompts.

        Returns:
            List of Prompt definitions

        Raises:
            McpRpcError: If the call fails
        """
        result = await self._call("prompts/list", {})
        prompts_data = result.get("prompts", [])
        return [Prompt.from_dict(p) for p in prompts_data]

    async def get_prompt(
        self,
        name: str,
        arguments: Optional[dict[str, Any]] = None,
    ) -> PromptResult:
        """Get a prompt template.

        Args:
            name: Prompt name
            arguments: Prompt arguments

        Returns:
            PromptResult with generated messages

        Raises:
            McpRpcError: If the call fails
        """
        params: dict[str, Any] = {"name": name}
        if arguments is not None:
            params["arguments"] = arguments

        result = await self._call("prompts/get", params)
        return PromptResult.from_dict(result)

    # ========== Utility methods ==========

    async def ping(self) -> bool:
        """Check if the server is responsive.

        Returns:
            True if server is responsive
        """
        try:
            if self._client is None:
                return False
            response = await self._client.get(f"{self.server_url}/health")
            return response.status_code == 200
        except Exception:
            return False

    def get_tool_names(self) -> list[str]:
        """Get cached tool names.

        This is a convenience method for quickly checking available tools
        without making a network call.

        Returns:
            List of tool names (empty if not yet fetched)
        """
        # This would require caching - for now return empty
        return []

    def get_resource_uris(self) -> list[str]:
        """Get cached resource URIs.

        This is a convenience method for quickly checking available resources
        without making a network call.

        Returns:
            List of resource URIs (empty if not yet fetched)
        """
        # This would require caching - for now return empty
        return []

    def get_prompt_names(self) -> list[str]:
        """Get cached prompt names.

        This is a convenience method for quickly checking available prompts
        without making a network call.

        Returns:
            List of prompt names (empty if not yet fetched)
        """
        # This would require caching - for now return empty
        return []
