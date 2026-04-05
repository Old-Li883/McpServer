"""Prompt builder for LLM interactions.

This module provides utilities for constructing prompts that include
tool descriptions, resource information, and conversation history.
"""

import json
from typing import Any, Optional

from agent.mcp.types import Tool


class PromptBuilder:
    """Builder for constructing LLM prompts with tool information."""

    def __init__(
        self,
        system_prompt: str = "You are a helpful AI assistant.",
    ):
        """Initialize the prompt builder.

        Args:
            system_prompt: Base system prompt to use
        """
        self.system_prompt = system_prompt
        self.tools: list[Tool] = []

    def set_tools(self, tools: list[Tool]) -> None:
        """Set the available tools.

        Args:
            tools: List of available tools
        """
        self.tools = tools

    def build_system_prompt(self, include_tools: bool = True) -> str:
        """Build the system prompt.

        Args:
            include_tools: Whether to include tool descriptions

        Returns:
            The complete system prompt
        """
        prompt = self.system_prompt

        if include_tools and self.tools:
            prompt += "\n\n## Available Tools\n\n"
            prompt += self._format_tools()

            prompt += "\n\n## Tool Use Guidelines\n\n"
            prompt += "**Required Tool Call Format:**\n\n"
            prompt += "When you need to use a tool, you MUST use the following exact format:\n\n"
            prompt += "For a single tool:\n"
            prompt += "```json\n"
            prompt += '{"tool": "tool_name", "arguments": {"param1": "value1", "param2": "value2"}}\n'
            prompt += "```\n\n"
            prompt += "For multiple tools:\n"
            prompt += "```json\n"
            prompt += '[\n'
            prompt += '  {"tool": "tool_name1", "arguments": {"param1": "value1"}},\n'
            prompt += '  {"tool": "tool_name2", "arguments": {"param2": "value2"}}\n'
            prompt += ']\n'
            prompt += "```\n\n"
            prompt += "**Important Rules:**\n"
            prompt += "1. You MUST use the exact JSON format shown above for ALL tool calls\n"
            prompt += "2. The JSON MUST be wrapped in ```json ... ``` code blocks\n"
            prompt += "3. For tools with no parameters, use empty object: {\"tool\": \"tool_name\", \"arguments\": {}}\n"
            prompt += "4. Do NOT use any other format (function calls, natural language, etc.)\n"
            prompt += "5. Always explain what you're doing before/after the tool call\n\n"

        return prompt

    def _format_tools(self) -> str:
        """Format the tool list for inclusion in the prompt.

        Returns:
            Formatted tool descriptions
        """
        if not self.tools:
            return "No tools available."

        parts = []
        for tool in self.tools:
            parts.append(f"### {tool.name}\n")
            parts.append(f"{tool.description}\n")

            if tool.input_schema.properties:
                parts.append("\n**Parameters:**\n")
                for param_name, param_schema in tool.input_schema.properties.items():
                    param_type = param_schema.get("type", "any")
                    param_desc = param_schema.get("description", "")
                    required = param_name in tool.input_schema.required
                    req_marker = " (required)" if required else " (optional)"

                    parts.append(f"- `{param_name}` ({param_type}){req_marker}")
                    if param_desc:
                        parts.append(f": {param_desc}")
                    parts.append("\n")

            parts.append("\n")

        return "".join(parts)

    def build_messages(
        self,
        user_message: str,
        conversation_history: list[dict[str, Any]],
        include_tools: bool = True,
    ) -> list[dict[str, Any]]:
        """Build the complete message list for chat API.

        Args:
            user_message: The current user message
            conversation_history: Previous conversation messages
            include_tools: Whether to include tool descriptions

        Returns:
            List of message dicts for the chat API
        """
        messages = []

        # Add system prompt
        messages.append({
            "role": "system",
            "content": self.build_system_prompt(include_tools=include_tools),
        })

        # Add conversation history
        messages.extend(conversation_history)

        # Add current user message
        messages.append({
            "role": "user",
            "content": user_message,
        })

        return messages

    def build_prompt(
        self,
        user_message: str,
        conversation_history: Optional[list[dict[str, Any]]] = None,
        include_tools: bool = True,
    ) -> str:
        """Build a single prompt string (for non-chat APIs).

        Args:
            user_message: The current user message
            conversation_history: Previous conversation messages
            include_tools: Whether to include tool descriptions

        Returns:
            The complete prompt string
        """
        prompt = self.build_system_prompt(include_tools=include_tools)

        if conversation_history:
            prompt += "\n\n## Conversation History\n\n"
            for msg in conversation_history:
                role = msg.get("role", "user").capitalize()
                content = msg.get("content", "")
                prompt += f"**{role}**: {content}\n\n"

        prompt += "\n## Current User Message\n\n"
        prompt += user_message

        return prompt


def format_tool_call(
    tool_name: str,
    arguments: dict[str, Any],
) -> str:
    """Format a tool call for display to the user.

    Args:
        tool_name: Name of the tool being called
        arguments: Tool arguments

    Returns:
        Formatted tool call string
    """
    args_str = json.dumps(arguments, indent=2)
    return f"Calling tool `{tool_name}` with arguments:\n{args_str}"


def format_tool_result(
    tool_name: str,
    result: str,
    is_error: bool = False,
) -> str:
    """Format a tool result for display to the user.

    Args:
        tool_name: Name of the tool that was called
        result: The tool's result
        is_error: Whether the result is an error

    Returns:
        Formatted tool result string
    """
    status = "Error" if is_error else "Result"
    return f"Tool `{tool_name}` {status}:\n{result}"
