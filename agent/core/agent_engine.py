"""Agent engine - Core orchestration logic for the MCP Agent.

This module provides the main AgentEngine class that coordinates
conversation, tools, RAG, and LLM interactions.
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

# RAG imports (optional)
try:
    from agent.rag import RAGEngine, RAGConfig
    RAG_AVAILABLE = True
except ImportError:
    RAG_AVAILABLE = False

# Memory imports (optional)
try:
    from agent.memory.integration.memory_orchestrator import MemoryOrchestrator
    MEMORY_AVAILABLE = True
except ImportError:
    MEMORY_AVAILABLE = False


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

        # RAG integration (optional)
        self._rag_engine: Optional[RAGEngine] = None
        self._rag_enabled = False
        self._rag_always_enabled: bool = False  # Always use RAG for all queries
        self._rag_context_cache: dict[str, str] = {}  # Query -> context cache

        # Memory integration (optional)
        self._memory_orchestrator: Optional[MemoryOrchestrator] = None
        self._memory_enabled = False

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

        # Initialize RAG if enabled
        if RAG_AVAILABLE and hasattr(self.config, 'rag') and self.config.rag.enabled:
            await self._initialize_rag()

        # Initialize Memory if enabled
        if MEMORY_AVAILABLE and hasattr(self.config, 'memory') and self.config.memory.long_term_enabled:
            await self._initialize_memory()

    async def process_message(
        self,
        user_message: str,
        max_tool_iterations: int = 5,
        use_rag: Optional[bool] = None,
    ) -> str:
        """Process a user message and generate a response.

        Args:
            user_message: The user's message
            max_tool_iterations: Maximum number of tool call iterations
            use_rag: Whether to use RAG (None = auto-detect)

        Returns:
            The agent's response

        Raises:
            Exception: If processing fails
        """
        # Add to short-term memory
        if self._memory_enabled:
            import uuid
            await self._memory_orchestrator.add_conversation_message(
                message_id=str(uuid.uuid4()),
                role="user",
                content=user_message,
            )

        # Retrieve relevant memories
        memory_context = ""
        if self._memory_enabled:
            try:
                memory_result = await self._memory_orchestrator.retrieve_relevant_memories(
                    query=user_message,
                    top_k=self.config.memory.retrieval_top_k,
                )

                if memory_result.memories:
                    memory_context = self._memory_orchestrator.format_memories_for_llm(memory_result)
            except Exception as e:
                print(f"Memory retrieval failed: {e}")

        # RAG enhancement (if enabled and applicable)
        rag_context = ""
        if self._rag_enabled and (use_rag is None or use_rag):
            should_use_rag = use_rag if use_rag is not None else self._should_use_rag(user_message)
            if should_use_rag:
                rag_context = await self._rag_enhance_query(user_message)

        # Combine all contexts
        enhanced_message = user_message
        contexts = []

        if memory_context:
            contexts.append(memory_context)
        if rag_context:
            contexts.append(rag_context)

        if contexts:
            enhanced_message = "\n\n".join(contexts) + f"\n\nUser question: {user_message}"

        # Add user message to conversation
        self.conversation.add_user_message(enhanced_message)

        # Process with potential tool calls
        response = await self._process_with_tools(max_tool_iterations, user_message)

        # Add assistant response to short-term memory
        if self._memory_enabled:
            import uuid
            await self._memory_orchestrator.add_conversation_message(
                message_id=str(uuid.uuid4()),
                role="assistant",
                content=response,
            )

            # Auto-save important memories to long-term storage
            if self.config.memory.auto_save_to_long_term:
                try:
                    saved_count = await self._memory_orchestrator.save_important_memories()
                    if saved_count > 0:
                        print(f"Saved {saved_count} memories to long-term storage")
                except Exception as e:
                    print(f"Failed to save memories: {e}")

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
            "rag_enabled": self._rag_enabled,
            "memory_enabled": self._memory_enabled,
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

        # Get RAG capabilities
        if self._rag_enabled and self._rag_engine:
            capabilities["rag"] = self._rag_engine.get_stats()

        # Get Memory capabilities
        if self._memory_enabled and self._memory_orchestrator:
            capabilities["memory"] = self._memory_orchestrator.get_stats()

        return capabilities

    # ========== Memory Integration Methods ==========

    async def _initialize_memory(self) -> None:
        """Initialize the memory system.

        Raises:
            Exception: If memory initialization fails
        """
        if not MEMORY_AVAILABLE:
            raise Exception("Memory module not available")

        # Create and initialize memory orchestrator
        self._memory_orchestrator = MemoryOrchestrator(
            short_term_max_messages=self.config.memory.short_term_max_messages,
            long_term_vector_db_path=self.config.memory.long_term_vector_db_path,
            long_term_embedder_model=self.config.memory.long_term_embedder_model,
            auto_save_threshold=self.config.memory.auto_save_importance_threshold,
        )

        await self._memory_orchestrator.initialize()
        self._memory_enabled = True

    async def create_memory(
        self,
        content: str,
        memory_type: str,
        importance: str = "important",
    ) -> dict:
        """Manually create a memory.

        Args:
            content: Memory content
            memory_type: Type of memory (user_preference/task_context/knowledge)
            importance: Importance level (critical/important/normal/trivial)

        Returns:
            Created memory as dictionary
        """
        if not self._memory_enabled:
            raise Exception("Memory system is not enabled")

        from agent.memory.types import MemoryType

        memory = await self._memory_orchestrator.create_manual_memory(
            content=content,
            memory_type=MemoryType(memory_type),
            importance=importance,
        )

        return memory.to_dict()

    async def search_memories(self, query: str, top_k: int = 5) -> list[dict]:
        """Search memories.

        Args:
            query: Search query
            top_k: Number of results

        Returns:
            List of memories as dictionaries
        """
        if not self._memory_enabled:
            raise Exception("Memory system is not enabled")

        memories = await self._memory_orchestrator.search_memories(query, top_k)
        return [m.to_dict() for m in memories]

    def get_memory_stats(self) -> dict:
        """Get memory system statistics.

        Returns:
            Dictionary with memory statistics
        """
        if not self._memory_enabled:
            return {"enabled": False}

        return self._memory_orchestrator.get_stats()

    async def clear_conversation(self) -> None:
        """Clear the conversation history."""
        self.conversation.clear()

        # Also clear short-term memory
        if self._memory_enabled:
            await self._memory_orchestrator.clear_conversation()

    # ========== RAG Integration Methods ==========

    async def _initialize_rag(self) -> None:
        """Initialize the RAG engine.

        Raises:
            Exception: If RAG initialization fails
        """
        if not RAG_AVAILABLE:
            raise Exception("RAG module not available")

        # Get RAG config (filter out 'enabled' field as RAGConfig doesn't have it)
        rag_config_dict = self.config.rag.model_dump() if hasattr(self.config.rag, 'model_dump') else {}
        rag_config_dict.pop('enabled', None)  # Remove 'enabled' field
        rag_config = RAGConfig(**rag_config_dict)

        # Store the always_enabled setting
        self._rag_always_enabled = rag_config.always_enabled

        # Create and initialize RAG engine
        self._rag_engine = RAGEngine(config=rag_config)
        await self._rag_engine.initialize()
        self._rag_enabled = True

    def _should_use_rag(self, message: str) -> bool:
        """Determine if RAG should be used for this message.

        Args:
            message: The user message

        Returns:
            True if RAG should be used
        """
        if not self._rag_enabled:
            return False

        # If always_enabled is True, always use RAG
        if self._rag_always_enabled:
            return True

        # Simple heuristic: use RAG for questions
        # Questions often end with question marks or contain question words
        message_lower = message.lower()

        # Chinese question words
        chinese_question_words = ['什么', '如何', '怎么', '为什么', '哪个', '哪些', '是否', '有没有']
        # English question words
        english_question_words = ['what', 'how', 'why', 'where', 'when', 'who', 'which', 'is', 'are', 'do', 'does', 'can', 'could', 'should', 'would']

        # Check for question marks
        if '?' in message or '？' in message:
            return True

        # Check for question words
        has_question_word = any(word in message_lower for word in english_question_words + chinese_question_words)
        if has_question_word:
            return True

        # Check for knowledge-related keywords
        knowledge_keywords = ['explain', 'tell me about', 'describe', 'documentation', '说明', '解释', '介绍']
        has_keyword = any(keyword in message_lower for keyword in knowledge_keywords)
        if has_keyword:
            return True

        return False

    async def _rag_enhance_query(self, query: str) -> str:
        """Enhance query with RAG context.

        Args:
            query: The user query

        Returns:
            Enhanced query with RAG context
        """
        if not self._rag_engine:
            return ""

        try:
            # Check cache first
            if query in self._rag_context_cache:
                return self._rag_context_cache[query]

            # Query RAG engine
            result = await self._rag_engine.query(query, top_k=3)

            if result.has_sources:
                # Use the RAG engine's answer which already contains formatted context
                # This includes document content, scores, and source references
                context = result.answer

                # Cache the context
                self._rag_context_cache[query] = context

                return context

        except Exception as e:
            # Log error but don't fail
            print(f"RAG query failed: {e}")

        return ""

    async def rag_add_documents(self, source: str, **kwargs) -> int:
        """Add documents to the RAG knowledge base.

        Args:
            source: Source path (file, directory, URL)
            **kwargs: Additional parameters

        Returns:
            Number of chunks added
        """
        if not self._rag_engine:
            raise Exception("RAG is not enabled")

        return await self._rag_engine.add_documents(source, **kwargs)

    async def rag_query(self, query: str, top_k: int = 5):
        """Direct RAG query (bypasses normal agent processing).

        Args:
            query: The query
            top_k: Number of results

        Returns:
            QueryResult
        """
        if not self._rag_engine:
            raise Exception("RAG is not enabled")

        return await self._rag_engine.query(query, top_k=top_k)

    async def rag_clear(self) -> None:
        """Clear all documents from the RAG knowledge base."""
        if not self._rag_engine:
            raise Exception("RAG is not enabled")

        await self._rag_engine.delete_collection()
        self._rag_context_cache.clear()

    def get_rag_stats(self) -> dict[str, Any]:
        """Get RAG statistics.

        Returns:
            RAG statistics dictionary
        """
        if not self._rag_engine:
            return {"enabled": False}

        return self._rag_engine.get_stats()

    # ========== End RAG Integration Methods ==========


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
