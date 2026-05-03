"""Response parser for LLM outputs.

This module provides utilities for parsing LLM responses to extract
tool calls in a standardized JSON format.
"""

import json
import re
from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class ToolCall:
    """Parsed tool call from LLM response."""

    name: str
    arguments: dict[str, Any]

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return {"name": self.name, "arguments": self.arguments}


@dataclass
class ParsedResponse:
    """Parsed LLM response."""

    text: str
    tool_calls: list[ToolCall]

    @property
    def has_tool_calls(self) -> bool:
        """Check if response contains tool calls."""
        return len(self.tool_calls) > 0


class ResponseParser:
    """Parser for LLM responses with JSON tool call format.

    The parser expects tool calls in the following format:
    - Single tool: ```json {"tool": "name", "arguments": {...}} ```
    - Multiple tools: ```json [{"tool": "name1", ...}, {"tool": "name2", ...}] ```
    """

    # Pattern for JSON code blocks: ```json {...}```
    TOOL_CALL_JSON_PATTERN = re.compile(
        r'```json\s*(\[.*?\]|\{.*?\})\s*```',
        re.MULTILINE | re.DOTALL,
    )

    def parse(self, response: str, available_tools: list[str] = None) -> ParsedResponse:
        """Parse an LLM response for tool calls.

        Args:
            response: The raw LLM response text
            available_tools: Optional list of available tool names for validation

        Returns:
            ParsedResponse with text and any tool calls
        """
        tool_calls = self._parse_json_tool_calls(response, available_tools or [])
        clean_text = self._clean_response(response, tool_calls)
        return ParsedResponse(text=clean_text, tool_calls=tool_calls)

    def _parse_json_tool_calls(self, response: str, available_tools: list[str]) -> list[ToolCall]:
        """Parse JSON code blocks for tool calls.

        Args:
            response: The LLM response text
            available_tools: List of available tool names for validation

        Returns:
            List of parsed ToolCall objects
        """
        calls = []

        for match in self.TOOL_CALL_JSON_PATTERN.finditer(response):
            json_str = match.group(1)
            try:
                data = json.loads(json_str)

                # Handle both single object and array formats
                items = data if isinstance(data, list) else [data]

                for item in items:
                    if not isinstance(item, dict):
                        continue

                    tool_name = item.get("tool")
                    arguments = item.get("arguments", {})

                    if not tool_name:
                        continue

                    # Validate tool exists (optional, can be skipped for flexibility)
                    # if available_tools and tool_name not in available_tools:
                    #     continue

                    if not isinstance(arguments, dict):
                        arguments = {}

                    calls.append(ToolCall(name=tool_name, arguments=arguments))

            except json.JSONDecodeError:
                continue

        return calls

    def _clean_response(self, response: str, tool_calls: list[ToolCall]) -> str:
        """Remove tool call blocks from the response text.

        Args:
            response: Original response text
            tool_calls: Parsed tool calls

        Returns:
            Cleaned response text
        """
        text = response

        # Remove JSON tool call blocks
        text = self.TOOL_CALL_JSON_PATTERN.sub("", text)

        # Clean up extra whitespace
        text = text.strip()
        text = re.sub(r"\n{3,}", "\n\n", text)

        return text

    def extract_code_blocks(self, response: str, language: Optional[str] = None) -> list[str]:
        """Extract code blocks from the response.

        Args:
            response: The response text
            language: Optional language filter (e.g., "python", "javascript")

        Returns:
            List of code block contents
        """
        if language:
            pattern = rf"```{language}\n(.*?)```"
            blocks = [match.group(1) for match in re.finditer(pattern, response, re.MULTILINE | re.DOTALL)]
        else:
            pattern = r"```(\w*)\n(.*?)```"
            blocks = [match.group(2) for match in re.finditer(pattern, response, re.MULTILINE | re.DOTALL)]

        return blocks

    def extract_json(self, response: str) -> Optional[dict[str, Any]]:
        """Extract a JSON object from the response.

        Args:
            response: The response text

        Returns:
            Parsed JSON dict or None if not found
        """
        # Try to find JSON objects in the response
        pattern = r"\{[^{}]*\}|\{[^{}]*\{[^{}]*\}[^{}]*\}"

        for match in re.finditer(pattern, response, re.MULTILINE | re.DOTALL):
            try:
                return json.loads(match.group(0))
            except json.JSONDecodeError:
                continue

        return None

    def has_refusal(self, response: str) -> bool:
        """Check if the response contains a refusal.

        Args:
            response: The response text

        Returns:
            True if response contains refusal language
        """
        refusal_patterns = [
            r"\bi (?:can't|cannot|won't|will not)\b",
            r"\bunable to\b",
            r"\bnot (?:allowed|able|permitted)\b",
            r"\bi don'?t (?:know|have information)\b",
            r"\bi'?m (?:not sure|uncertain)\b",
        ]

        response_lower = response.lower()
        for pattern in refusal_patterns:
            if re.search(pattern, response_lower):
                return True

        return False
