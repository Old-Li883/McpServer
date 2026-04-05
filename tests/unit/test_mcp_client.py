"""Unit tests for MCP client."""

import pytest

from agent.mcp.client import McpClient, McpConnectionError, McpRpcError
from agent.mcp.types import Tool


class TestMcpClient:
    """Test MCP client functionality."""

    @pytest.mark.asyncio
    async def test_initialize(self, mock_mcp_client: McpClient) -> None:
        """Test MCP initialization."""
        result = await mock_mcp_client.initialize()

        assert result is not None
        assert result.server_info == "test-mcp-server"
        assert result.version == "1.0.0"
        assert result.capabilities.tools is True
        assert result.capabilities.resources is True
        assert result.capabilities.prompts is True

    @pytest.mark.asyncio
    async def test_list_tools(self, mock_mcp_client: McpClient) -> None:
        """Test listing tools."""
        # First initialize
        await mock_mcp_client.initialize()

        # List tools
        tools = await mock_mcp_client.list_tools()

        assert len(tools) == 1
        assert tools[0].name == "test_tool"
        assert tools[0].description == "A test tool"

    @pytest.mark.asyncio
    async def test_call_tool(self, mock_mcp_client: McpClient) -> None:
        """Test calling a tool."""
        # First initialize
        await mock_mcp_client.initialize()

        # Call tool
        result = await mock_mcp_client.call_tool("test_tool", {"param1": "value"})

        assert result is not None
        assert result.is_error is False
        assert len(result.content) == 1
        assert result.content[0].text == "Tool execution successful"


class TestToolTypes:
    """Test MCP data types."""

    def test_tool_creation(self) -> None:
        """Test creating a Tool object."""
        from agent.mcp.types import ToolInputSchema

        schema = ToolInputSchema(
            type="object",
            properties={"param1": {"type": "string"}},
            required=["param1"],
        )

        tool = Tool(
            name="test_tool",
            description="Test description",
            input_schema=schema,
        )

        assert tool.name == "test_tool"
        assert tool.description == "Test description"
        assert tool.input_schema.type == "object"
        assert "param1" in tool.input_schema.properties

    def test_tool_serialization(self) -> None:
        """Test tool to_dict conversion."""
        from agent.mcp.types import ToolInputSchema

        schema = ToolInputSchema(properties={"param1": {"type": "string"}})
        tool = Tool(name="test", description="Test", input_schema=schema)

        data = tool.to_dict()

        assert data["name"] == "test"
        assert data["description"] == "Test"
        assert "inputSchema" in data
        assert data["inputSchema"]["type"] == "object"

    def test_content_item_text(self) -> None:
        """Test creating text content item."""
        from agent.mcp.types import ContentItem

        item = ContentItem.text_content("Hello world")

        assert item.type == "text"
        assert item.text == "Hello world"
        assert item.data is None
