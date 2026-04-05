"""Response parser for LLM outputs.

This module provides utilities for parsing LLM responses to extract
tool calls and structured outputs.
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
    """Parser for LLM responses."""

    # Pattern for tool calls: TOOL: tool_name\nARGUMENTS: {...}
    TOOL_CALL_PATTERN = re.compile(
        r"TOOL:\s*(\w+)\s*\nARGUMENTS:\s*(\{.*?\})",
        re.MULTILINE | re.DOTALL,
    )

    # Alternative pattern for inline tool calls: ```TOOL: tool_name ARGUMENTS: {...}```
    INLINE_TOOL_PATTERN = re.compile(
        r"```?\s*TOOL:\s*(\w+)\s+ARGUMENTS:\s*(\{.*?\})\s*```?",
        re.MULTILINE | re.DOTALL,
    )

    # Pattern for JSON function calls (OpenAI style)
    FUNCTION_CALL_PATTERN = re.compile(
        r'<function_calls>\s*<invoke name="(\w+)">\s*<parameter name="([^"]+)">(.+?)</parameter>\s*</invoke>\s*</function_calls>',
        re.MULTILINE | re.DOTALL,
    )

    # Pattern for natural language tool calls: "use tool_name with ..." or "call tool_name"
    NATURAL_TOOL_PATTERN = re.compile(
        r'(?:use|call|invoke|execute)\s+(?:the\s+)?(?:tool\s+)?["\']?(\w+)["\']?(?:\s+(?:with|using)\s+(.+?))?(?:\.|$)',
        re.MULTILINE | re.IGNORECASE,
    )

    # Pattern for action-based tool calls: "I will get the current time" -> get_current_time
    ACTION_PATTERN = re.compile(
        r'(?:I\s+will|let\s+me|I\'?ll|going\s+to)\s+(?:get|fetch|calculate|check|echo|add|call)\s+(?:the\s+)?(.+?)(?:\.|$)',
        re.MULTILINE | re.IGNORECASE,
    )

    # Pattern for explicit function call style: get_current_time()
    FUNCTION_STYLE_PATTERN = re.compile(
        r'(\w+(?:_time|_info|_echo|_add))\s*\(\s*(\{.*?\}.*?)?\s*\)',
        re.MULTILINE | re.DOTALL,
    )

    # Pattern for OpenAI function calling format
    OPENAI_FUNCTION_PATTERN = re.compile(
        r'"name"\s*:\s*"(\w+)"',
        re.MULTILINE | re.DOTALL,
    )

    def parse(self, response: str, available_tools: list[str] = None) -> ParsedResponse:
        """Parse an LLM response.

        Args:
            response: The raw LLM response text
            available_tools: Optional list of available tool names for natural language matching

        Returns:
            ParsedResponse with text and any tool calls
        """
        tool_calls = []

        # Try to parse tool calls using various patterns (in order of specificity)
        tool_calls.extend(self._parse_tool_blocks(response))
        tool_calls.extend(self._parse_inline_tools(response))
        tool_calls.extend(self._parse_function_calls(response))
        tool_calls.extend(self._parse_natural_tools(response, available_tools or []))
        tool_calls.extend(self._parse_action_tools(response, available_tools or []))
        tool_calls.extend(self._parse_function_style(response))

        # Clean up the response text
        clean_text = self._clean_response(response, tool_calls)

        return ParsedResponse(text=clean_text, tool_calls=tool_calls)

    def _parse_tool_blocks(self, response: str) -> list[ToolCall]:
        """Parse tool call blocks."""
        calls = []

        for match in self.TOOL_CALL_PATTERN.finditer(response):
            tool_name = match.group(1)
            arguments_str = match.group(2)

            try:
                arguments = json.loads(arguments_str)
                calls.append(ToolCall(name=tool_name, arguments=arguments))
            except json.JSONDecodeError:
                continue

        return calls

    def _parse_inline_tools(self, response: str) -> list[ToolCall]:
        """Parse inline tool calls."""
        calls = []

        for match in self.INLINE_TOOL_PATTERN.finditer(response):
            tool_name = match.group(1)
            arguments_str = match.group(2)

            try:
                arguments = json.loads(arguments_str)
                calls.append(ToolCall(name=tool_name, arguments=arguments))
            except json.JSONDecodeError:
                continue

        return calls

    def _parse_function_calls(self, response: str) -> list[ToolCall]:
        """Parse function call format (OpenAI style)."""
        calls = []

        # This is a simplified parser for OpenAI-style function calls
        # Real implementation would need to handle XML parsing properly
        for match in self.FUNCTION_CALL_PATTERN.finditer(response):
            tool_name = match.group(1)
            # For now, skip this complex format
            # A full implementation would properly parse the XML structure

        return calls

    def _parse_natural_tools(self, response: str, available_tools: list[str]) -> list[ToolCall]:
        """Parse natural language tool calls like 'use get_current_time'."""
        calls = []

        for match in self.NATURAL_TOOL_PATTERN.finditer(response):
            tool_name = match.group(1)
            args_text = match.group(2) if match.lastindex >= 2 else ""

            # Check if this matches an available tool
            if available_tools and tool_name not in available_tools:
                # Try to find partial match
                for tool in available_tools:
                    if tool_name.lower() in tool.lower() or tool.lower() in tool_name.lower():
                        tool_name = tool
                        break

            # Try to parse arguments as JSON
            arguments = {}
            if args_text:
                # Try to find JSON in the arguments text
                json_match = re.search(r'\{.*?\}', args_text, re.DOTALL)
                if json_match:
                    try:
                        arguments = json.loads(json_match.group(0))
                    except json.JSONDecodeError:
                        # Parse key-value pairs naturally
                        arguments = self._parse_natural_arguments(args_text)

            calls.append(ToolCall(name=tool_name, arguments=arguments))

        return calls

    def _parse_action_tools(self, response: str, available_tools: list[str]) -> list[ToolCall]:
        """Parse action-based tool calls like 'I will get the current time'."""
        calls = []

        if not available_tools:
            return calls

        response_lower = response.lower()

        # Action to tool mapping
        action_mappings = {
            'time': 'get_current_time',
            'current time': 'get_current_time',
            'date': 'get_current_time',
            'echo': 'echo',
            'repeat': 'echo',
            'say': 'echo',
            'add': 'add',
            'calculate': 'add',
            'sum': 'add',
            'plus': 'add',
        }

        for action, tool_name in action_mappings.items():
            if tool_name not in available_tools:
                continue

            # Look for action phrases
            if f'get the {action}' in response_lower or f'check the {action}' in response_lower:
                # Extract potential arguments
                arguments = {}
                if tool_name == 'echo':
                    # Look for quoted text after echo/say
                    echo_match = re.search(r'(?:echo|say|repeat)\s+[\'"](.+?)[\'"]', response_lower)
                    if echo_match:
                        arguments = {'text': echo_match.group(1)}
                elif tool_name == 'add':
                    # Look for numbers
                    numbers = re.findall(r'\d+\.?\d*', response)
                    if len(numbers) >= 2:
                        arguments = {'a': float(numbers[0]), 'b': float(numbers[1])}

                calls.append(ToolCall(name=tool_name, arguments=arguments))
                break  # Only take the first match

        return calls

    def _parse_function_style(self, response: str) -> list[ToolCall]:
        """Parse function-style calls like get_current_time()."""
        calls = []

        for match in self.FUNCTION_STYLE_PATTERN.finditer(response):
            tool_name = match.group(1)
            args_content = match.group(2) if match.lastindex >= 2 else ""

            # Parse arguments
            arguments = {}
            if args_content:
                # Try JSON first
                try:
                    arguments = json.loads(args_content)
                except json.JSONDecodeError:
                    # Try key=value format
                    arguments = self._parse_natural_arguments(args_content)

            calls.append(ToolCall(name=tool_name, arguments=arguments))

        return calls

    def _parse_natural_arguments(self, text: str) -> dict[str, Any]:
        """Parse natural language arguments into a dict."""
        arguments = {}

        # Try to extract key-value pairs
        # Format: "key=value" or "key: value"
        patterns = [
            r'(\w+)\s*=\s*["\']?([^"\'\s,]+)["\']?',
            r'(\w+)\s*:\s*["\']?([^"\'\s,]+)["\']?',
        ]

        for pattern in patterns:
            for match in re.finditer(pattern, text):
                key, value = match.groups()
                # Try to convert to number
                try:
                    if '.' in value:
                        value = float(value)
                    else:
                        value = int(value)
                except (ValueError, TypeError):
                    pass  # Keep as string
                arguments[key] = value
                if len(arguments) >= 5:  # Limit to prevent over-parsing
                    break

        return arguments

    def _clean_response(self, response: str, tool_calls: list[ToolCall]) -> str:
        """Remove tool call blocks from the response text.

        Args:
            response: Original response text
            tool_calls: Parsed tool calls

        Returns:
            Cleaned response text
        """
        text = response

        # Remove tool call blocks
        text = self.TOOL_CALL_PATTERN.sub("", text)
        text = self.INLINE_TOOL_PATTERN.sub("", text)

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
        else:
            pattern = r"```(\w*)\n(.*?)```"

        blocks = []
        for match in re.finditer(pattern, response, re.MULTILINE | re.DOTALL):
            blocks.append(match.group(2))

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
