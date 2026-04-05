"""Agent engine - Core orchestration logic for the MCP Agent.

This module provides the main AgentEngine class that coordinates
conversation, tools, and LLM interactions.
"""

import asyncio
from typing import Any, Optional

from agent.config import Config
from agent.core.conversation import Conversation, Message, Role
from agent.core.tools import ToolOrchestrator
from agent.llm.ollama_client import OllamaClient
from agent.llm.prompt_builder import PromptBuilder
from agent.llm.response_parser import ParsedResponse, ToolCall
from agent.mcp.client import McpClient
from agent.mcp.types import ToolResult


class AgentEngine:
    """Core agent engine that orchestrates all components.

    This class manages the interaction between the user, LLM, and MCP tools,
    handling conversation state, tool execution, and response generation.
    """

    def __init__(
        self,
        config: Config,
        mcp_client: McpClient,
        llm_client: OllamaClient,
    ):
        """Initialize the agent engine.

        Args:
            config: Agent configuration
            mcp_client: MCP client for tool/resource/prompt access
            llm_client: LLM client for generation
        """
        self.config = config
        self.mcp_client = mcp_client
        self.llm_client = llm_client

        # Initialize components
        self.conversation = Conversation(max_history=config.conversation.max_history)
        self.conversation.set_system_prompt(config.conversation.system_prompt)

        self.tool_orchestrator = ToolOrchestrator(mcp_client)
        self.prompt_builder = PromptBuilder(system_prompt=config.conversation.system_prompt)

        self._tools_loaded = False

    async def initialize(self) -> None:
        """Initialize the agent engine.

        This loads available tools and prepares the agent for interaction.

        Raises:
            Exception: If initialization fails
        """
        # Load available tools
        tools = await self.tool_orchestrator.load_tools()
        self.prompt_builder.set_tools(tools)
        self._tools_loaded = True

    async def process_message(
        self,
        user_message: str,
        max_tool_iterations: int = 5,
    ) -> str:
        """Process a user message and generate a response.

        Args:
            user_message: The user's message
            max_tool_iterations: Maximum number of tool call iterations

        Returns:
            The agent's response

        Raises:
            Exception: If processing fails
        """
        # Add user message to conversation
        self.conversation.add_user_message(user_message)

        # Process with potential tool calls
        response = await self._process_with_tools(max_tool_iterations, user_message)

        return response

    async def _process_with_tools(
        self,
        max_iterations: int,
        original_user_message: str = "",
    ) -> str:
        """Process with tool calling support.

        Args:
            max_iterations: Maximum tool call iterations
            original_user_message: Original user message for tool inference

        Returns:
            The final response text
        """
        iteration = 0
        final_response = ""
        used_tools = set()  # Track which tools have been used

        while iteration < max_iterations:
            iteration += 1

            # Get current conversation context
            messages = self.conversation.get_messages_for_llm()

            # Generate response from LLM
            llm_response = await self._generate_response(messages)

            # Parse for tool calls (with available tools context)
            available_tool_names = self.tool_orchestrator.get_tool_names()
            parsed = self.tool_orchestrator.parse_tool_calls(llm_response, available_tool_names)

            # If no tool calls detected, try to infer tools from the response
            # Only do inference on first iteration
            if not parsed.has_tool_calls and iteration == 1 and original_user_message:
                inferred_calls = self._infer_tool_calls(original_user_message, available_tool_names)
                if inferred_calls:
                    # Filter out already used tools
                    new_calls = [c for c in inferred_calls if c.name not in used_tools]
                    if new_calls:
                        parsed.tool_calls.extend(new_calls)

            # Add assistant response to conversation
            self.conversation.add_assistant_message(
                parsed.text,
                tool_calls=[tc.to_dict() for tc in parsed.tool_calls] if parsed.tool_calls else None,
            )

            # Re-check after potential inference
            if not parsed.tool_calls:
                # If LLM returned empty text but we have tool results, use them
                if not parsed.text.strip() and used_tools:
                    # Get the last tool result as the response
                    for msg in reversed(self.conversation.get_messages()):
                        if msg.role == Role.TOOL:
                            # Clean up the tool result format
                            final_response = msg.content
                            # Remove [Tool: xxx] prefix if present
                            if '[' in final_response and ']' in final_response:
                                final_response = final_response.split(']', 1)[-1].strip()
                            break
                    if not final_response:
                        final_response = "Tool execution completed."
                else:
                    final_response = parsed.text if parsed.text.strip() else "I apologize, but I couldn't generate a proper response."
                break

            # Execute tools
            for tool_call in parsed.tool_calls:
                # Skip if this tool was already used
                if tool_call.name in used_tools:
                    continue

                used_tools.add(tool_call.name)
                try:
                    result = await self.tool_orchestrator.execute_tool(
                        tool_call.name,
                        tool_call.arguments,
                    )

                    # Format and add tool result to conversation
                    result_text = self.tool_orchestrator.format_tool_result(
                        tool_call.name,
                        result,
                    )
                    self.conversation.add_tool_message(tool_call.name, result_text)

                except Exception as e:
                    # Add error as tool result
                    self.conversation.add_tool_message(
                        tool_call.name,
                        f"Error: {str(e)}",
                    )

        if iteration >= max_iterations:
            final_response = "I reached the maximum number of tool iterations. Let me summarize what I found."

        return final_response

    async def _generate_response(self, messages: list[dict[str, Any]]) -> str:
        """Generate a response using the LLM.

        Args:
            messages: Conversation messages

        Returns:
            Generated response text
        """
        try:
            response = await self.llm_client.chat(
                messages=messages,
                temperature=self.config.llm.temperature,
                max_tokens=self.config.llm.max_tokens,
            )
            return response
        except Exception as e:
            return f"I encountered an error generating a response: {str(e)}"

    def _infer_tool_calls(self, user_message: str, available_tools: list[str]) -> list[ToolCall]:
        """Infer tool calls from user message when LLM doesn't call tools.

        This is a fallback mechanism to ensure tools are used when appropriate.

        Args:
            user_message: The original user message
            available_tools: List of available tool names

        Returns:
            List of inferred tool calls
        """
        import re
        from agent.llm.response_parser import ToolCall

        calls = []
        message_lower = user_message.lower()

        # Tool inference patterns
        tool_patterns = {
            'get_current_time': [
                r'\b(time|current time|date|what time|what\'s the time)\b',
                r'\b(now|today|clock)\b'
            ],
            'echo': [
                r'\b(echo|repeat|say again|tell me again)\b',
                r'\b(say\s+[\'"].*?[\'"])'
            ],
            'add': [
                r'\b(add|plus|sum|calculate|total)\b.*\d+.*\d+',
                r'\d+\s*[\+]\s*\d+',
            ]
        }

        # Check each tool
        for tool_name, patterns in tool_patterns.items():
            if tool_name not in available_tools:
                continue

            for pattern in patterns:
                if re.search(pattern, message_lower, re.IGNORECASE):
                    arguments = {}

                    # Extract tool-specific arguments
                    if tool_name == 'echo':
                        # Extract quoted text or text after echo/say
                        echo_match = re.search(r'(?:echo|repeat|say)\s+[\'"](.+?)[\'"]', user_message, re.IGNORECASE)
                        if echo_match:
                            arguments = {'text': echo_match.group(1)}
                        else:
                            # Use the whole message minus the command
                            text = re.sub(r'^(echo|repeat|say)\s*', '', user_message, flags=re.IGNORECASE)
                            arguments = {'text': text.strip()}

                    elif tool_name == 'add':
                        # Extract two numbers
                        numbers = re.findall(r'\d+\.?\d*', user_message)
                        if len(numbers) >= 2:
                            arguments = {'a': float(numbers[0]), 'b': float(numbers[1])}

                    elif tool_name == 'get_current_time':
                        # No arguments needed
                        arguments = {}

                    if arguments or tool_name == 'get_current_time':
                        calls.append(ToolCall(name=tool_name, arguments=arguments))
                        break  # Only add one call per tool

        return calls

    def get_conversation_history(self) -> list[Message]:
        """Get the conversation history.

        Returns:
            List of messages
        """
        return self.conversation.get_messages()

    def clear_conversation(self) -> None:
        """Clear the conversation history."""
        self.conversation.clear()

    def get_available_tools(self) -> list[str]:
        """Get the names of available tools.

        Returns:
            List of tool names
        """
        return self.tool_orchestrator.get_tool_names()

    async def get_capabilities(self) -> dict[str, Any]:
        """Get agent capabilities.

        Returns:
            Dict with capabilities info
        """
        capabilities = {
            "tools": self.get_available_tools(),
            "llm_model": self.config.llm.model,
        }

        # Get MCP server capabilities
        try:
            server_caps = self.mcp_client.capabilities
            capabilities["mcp_server"] = {
                "tools": server_caps.tools,
                "resources": server_caps.resources,
                "prompts": server_caps.prompts,
            }
        except Exception:
            capabilities["mcp_server"] = {"error": "Not connected"}

        return capabilities


async def create_agent(config: Optional[Config] = None) -> AgentEngine:
    """Create an initialized agent engine.

    Args:
        config: Optional configuration (uses defaults if not provided)

    Returns:
        Initialized AgentEngine

    Raises:
        Exception: If creation or initialization fails
    """
    if config is None:
        from agent.config import load_config
        config = load_config()

    # Create clients
    mcp_client = McpClient(
        server_url=config.mcp.server_url,
        timeout=config.mcp.timeout,
    )

    llm_client = OllamaClient(
        base_url=config.llm.base_url,
        model=config.llm.model,
        timeout=120.0,
    )

    # Connect clients
    await mcp_client.connect()
    await llm_client.connect()

    # Create and initialize agent
    agent = AgentEngine(config, mcp_client, llm_client)
    await agent.initialize()

    return agent
