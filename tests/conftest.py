"""Test configuration for MCP Agent tests."""

import asyncio
from pathlib import Path
from typing import AsyncGenerator

import pytest
from httpx import ASGITransport, Response

from agent.config import Config
from agent.mcp.client import McpClient
from agent.mcp.types import Tool


# Mock MCP server responses
MOCK_INITIALIZE_RESPONSE = {
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "protocolVersion": "2024-11-05",
        "capabilities": {
            "tools": {},
            "resources": {},
            "prompts": {},
        },
        "serverInfo": {
            "name": "test-mcp-server",
            "version": "1.0.0",
        },
    },
}

MOCK_TOOLS_LIST_RESPONSE = {
    "jsonrpc": "2.0",
    "id": 2,
    "result": {
        "tools": [
            {
                "name": "test_tool",
                "description": "A test tool",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "param1": {"type": "string", "description": "First parameter"},
                    },
                    "required": ["param1"],
                },
            }
        ]
    },
}

MOCK_TOOL_CALL_RESPONSE = {
    "jsonrpc": "2.0",
    "id": 3,
    "result": {
        "content": [
            {
                "type": "text",
                "text": "Tool execution successful",
            }
        ],
        "isError": False,
    },
}


@pytest.fixture
def config() -> Config:
    """Get test configuration."""
    return Config()


@pytest.fixture
async def mock_mcp_client() -> AsyncGenerator[McpClient, None]:
    """Create a mock MCP client for testing."""

    async def mock_handler(request):
        """Mock HTTP handler."""
        # Parse request body
        import json

        request_data = json.loads(request.content)

        # Return appropriate mock response
        if request_data.get("method") == "initialize":
            return Response(200, json=MOCK_INITIALIZE_RESPONSE)
        elif request_data.get("method") == "tools/list":
            return Response(200, json=MOCK_TOOLS_LIST_RESPONSE)
        elif request_data.get("method") == "tools/call":
            return Response(200, json=MOCK_TOOL_CALL_RESPONSE)
        else:
            return Response(200, json={"jsonrpc": "2.0", "id": request_data.get("id"), "result": {}})

    # Create mock transport
    transport = ASGITransport(mock_handler)

    # Create client with mock URL
    client = McpClient(server_url="http://test.local", timeout=5.0)
    client._client = client._client or __import__("httpx").AsyncClient(
        transport=transport, base_url="http://test.local"
    )

    yield client

    await client.close()


@pytest.fixture(autouse=True)
def reset_config(monkeypatch) -> None:
    """Reset config for each test."""
    # Use test config
    test_config = Path(__file__).parent.parent / "agent" / "config.yaml"
    monkeypatch.setattr("agent.config.load_config", lambda p=None: Config())
