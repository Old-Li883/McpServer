"""Tool orchestration for the MCP Agent.

This module handles tool discovery, execution, and result processing.
"""

import asyncio
import json
from typing import Any, Optional

from agent.llm.response_parser import ParsedResponse, ToolCall, ResponseParser
from agent.mcp.client import McpClient
from agent.mcp.types import Tool, ToolResult


class ToolExecutionError(Exception):
    """Exception raised when tool execution fails."""

    pass


class ToolOrchestrator:
    """Orchestrates tool discovery and execution.

    This class manages the interaction between the agent and available tools,
    including tool discovery, validation, and execution.
    """

    def __init__(
        self,
        mcp_client: McpClient,
        response_parser: Optional[ResponseParser] = None,
    ):
        """Initialize the tool orchestrator.

        Args:
            mcp_client: MCP client for tool execution
            response_parser: Optional response parser for detecting tool calls
        """
        self.mcp_client = mcp_client
        self.response_parser = response_parser or ResponseParser()
        self._available_tools: list[Tool] = []
        self._tools_loaded = False

    async def load_tools(self) -> list[Tool]:
        """Load available tools from the MCP server.

        Returns:
            List of available tools

        Raises:
            ToolExecutionError: If loading tools fails
        """
        try:
            self._available_tools = await self.mcp_client.list_tools()
            self._tools_loaded = True
            return self._available_tools
        except Exception as e:
            raise ToolExecutionError(f"Failed to load tools: {e}") from e

    def get_tools(self) -> list[Tool]:
        """Get the list of available tools.

        Returns:
            List of available tools (empty if not loaded)
        """
        return self._available_tools.copy()

    def get_tool(self, name: str) -> Optional[Tool]:
        """Get a specific tool by name.

        Args:
            name: Tool name

        Returns:
            Tool if found, None otherwise
        """
        for tool in self._available_tools:
            if tool.name == name:
                return tool
        return None

    def get_tool_names(self) -> list[str]:
        """Get the names of available tools.

        Returns:
            List of tool names
        """
        return [tool.name for tool in self._available_tools]

    def format_tools_for_prompt(self) -> str:
        """Format tools for inclusion in a prompt.

        Returns:
            Formatted tool descriptions
        """
        if not self._available_tools:
            return "No tools available."

        parts = []
        for tool in self._available_tools:
            parts.append(f"### {tool.name}\n{tool.description}\n")
            if tool.input_schema.properties:
                parts.append("\n**Parameters:**\n")
                for param_name, param_schema in tool.input_schema.properties.items():
                    param_type = param_schema.get("type", "any")
                    required = param_name in tool.input_schema.required
                    req_marker = " (required)" if required else " (optional)"
                    parts.append(f"- `{param_name}` ({param_type}){req_marker}\n")
                parts.append("\n")

        return "".join(parts)

    async def execute_tool(
        self,
        name: str,
        arguments: dict[str, Any],
    ) -> ToolResult:
        """Execute a tool.

        Args:
            name: Tool name
            arguments: Tool arguments

        Returns:
            ToolResult with the tool's output

        Raises:
            ToolExecutionError: If execution fails
        """
        # Check if tool exists
        tool = self.get_tool(name)
        if tool is None:
            raise ToolExecutionError(f"Tool '{name}' not found")

        # Validate arguments against schema
        self._validate_arguments(tool, arguments)

        # Execute the tool
        try:
            result = await self.mcp_client.call_tool(name, arguments)
            return result
        except Exception as e:
            raise ToolExecutionError(f"Tool execution failed: {e}") from e

    async def execute_tool_calls(
        self,
        tool_calls: list[ToolCall],
    ) -> list[tuple[ToolCall, ToolResult]]:
        """Execute multiple tool calls.

        Args:
            tool_calls: List of tool calls to execute

        Returns:
            List of (tool_call, result) tuples

        Raises:
            ToolExecutionError: If any execution fails
        """
        results = []

        for tool_call in tool_calls:
            try:
                result = await self.execute_tool(tool_call.name, tool_call.arguments)
                results.append((tool_call, result))
            except ToolExecutionError as e:
                # Create an error result
                error_result = ToolResult.error(str(e))
                results.append((tool_call, error_result))

        return results

    def parse_tool_calls(self, response: str, available_tools: list[str] = None) -> ParsedResponse:
        """Parse tool calls from an LLM response.

        Args:
            response: LLM response text
            available_tools: Optional list of available tool names for better parsing

        Returns:
            ParsedResponse with text and tool calls
        """
        return self.response_parser.parse(response, available_tools or [])

    def _validate_arguments(
        self,
        tool: Tool,
        arguments: dict[str, Any],
    ) -> None:
        """Validate arguments against the tool's schema.

        Args:
            tool: Tool definition
            arguments: Arguments to validate

        Raises:
            ToolExecutionError: If validation fails
        """
        schema = tool.input_schema

        # Check required parameters
        for required_param in schema.required:
            if required_param not in arguments:
                raise ToolExecutionError(
                    f"Missing required parameter '{required_param}' for tool '{tool.name}'"
                )

        # Check for unknown parameters (optional warning)
        # for param_name in arguments:
        #     if param_name not in schema.properties:
        #         # Unknown parameter - could log a warning here

    def format_tool_result(self, tool_name: str, result: ToolResult) -> str:
        """Format a tool result for display.

        Args:
            tool_name: Name of the tool
            result: Tool result

        Returns:
            Formatted result string
        """
        if result.is_error:
            return f"Tool '{tool_name}' returned an error."

        parts = []
        for item in result.content:
            if item.type == "text" and item.text:
                parts.append(item.text)
            elif item.type == "resource" and item.uri:
                parts.append(f"Resource: {item.uri}")

        return "\n".join(parts) if parts else "Tool returned no output."

    async def process_tool_calls_from_response(
        self,
        response: str,
    ) -> tuple[str, list[tuple[ToolCall, ToolResult]]]:
        """Process tool calls from an LLM response.

        Args:
            response: LLM response text

        Returns:
            Tuple of (cleaned_response_text, list_of_results)
        """
        parsed = self.parse_tool_calls(response)

        if not parsed.has_tool_calls:
            return response, []

        results = await self.execute_tool_calls(parsed.tool_calls)
        return parsed.text, results
