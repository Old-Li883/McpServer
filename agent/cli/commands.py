"""CLI command handling for the MCP Agent."""

import asyncio
from pathlib import Path
from typing import Optional

from agent.cli.display import Display
from agent.core.agent_engine import AgentEngine


class CLICommands:
    """Handler for CLI commands."""

    # Special commands
    COMMANDS = {
        "help": "Show available commands",
        "clear": "Clear conversation history",
        "tools": "List available tools",
        "caps": "Show agent capabilities",
        "quit": "Exit the agent",
        "exit": "Exit the agent (same as quit)",
        "multiline": "Toggle multiline input mode",
        "kb": "Knowledge base management (RAG)",
    }

    # KB commands
    KB_COMMANDS = {
        "add": "Add documents to knowledge base",
        "list": "List knowledge base collections",
        "stats": "Show knowledge base statistics",
        "search": "Search the knowledge base",
        "clear": "Clear all documents from knowledge base",
        "delete": "Delete a specific collection",
    }

    def __init__(
        self,
        agent: AgentEngine,
        display: Display,
    ):
        """Initialize the command handler.

        Args:
            agent: Agent engine instance
            display: Display instance
        """
        self.agent = agent
        self.display = display
        self.multiline_mode = False

    def is_command(self, input_text: str) -> bool:
        """Check if input is a command.

        Args:
            input_text: User input

        Returns:
            True if input is a command
        """
        text = input_text.strip().lower()
        return text in self.COMMANDS or text.startswith("/")

    async def execute(self, input_text: str) -> bool:
        """Execute a command.

        Args:
            input_text: User input command

        Returns:
            True if should continue running, False to exit
        """
        text = input_text.strip().lower()

        # Remove leading slash if present
        if text.startswith("/"):
            text = text[1:]

        if text in ("quit", "exit"):
            self.display.print_info("Goodbye!")
            return False

        if text == "help":
            self.display.print_help()

        elif text == "clear":
            self.agent.clear_conversation()
            self.display.print_success("Conversation history cleared.")

        elif text == "tools":
            tools = self.agent.get_available_tools()
            self.display.print_tool_list(tools)

        elif text == "caps":
            caps = await self.agent.get_capabilities()
            self.display.print_capabilities(caps)

        elif text == "multiline":
            self.multiline_mode = not self.multiline_mode
            status = "enabled" if self.multiline_mode else "disabled"
            self.display.print_info(f"Multiline mode {status}.")

        elif text.startswith("kb "):
            # Knowledge base commands
            return await self._handle_kb_command(input_text)

        else:
            self.display.print_error(f"Unknown command: {input_text}")
            self.display.print_info("Type 'help' for available commands.")

        return True

    async def _handle_kb_command(self, input_text: str) -> bool:
        """Handle knowledge base commands.

        Args:
            input_text: User input command

        Returns:
            True to continue running
        """
        # Parse command
        parts = input_text.strip().split()
        if len(parts) < 2:
            self.display.print_error("Usage: /kb <command> [arguments]")
            self.display.print_info("Available commands: " + ", ".join(self.KB_COMMANDS.keys()))
            return True

        command = parts[1]

        if command == "add":
            return await self._kb_add_command(parts[2:])

        elif command == "list":
            return await self._kb_list_command()

        elif command == "stats":
            return await self._kb_stats_command()

        elif command == "search":
            return await self._kb_search_command(parts[2:])

        elif command == "clear":
            return await self._kb_clear_command()

        elif command == "delete":
            return await self._kb_delete_command(parts[2:])

        else:
            self.display.print_error(f"Unknown kb command: {command}")
            self.display.print_info("Available commands: " + ", ".join(self.KB_COMMANDS.keys()))

        return True

    async def _kb_add_command(self, args: list[str]) -> bool:
        """Handle /kb add command.

        Usage: /kb add <path> [strategy]
        """
        if not args:
            self.display.print_error("Usage: /kb add <path> [strategy]")
            self.display.print_info("Strategies: auto, fixed, semantic, markdown, code")
            return True

        source = args[0]
        strategy = args[1] if len(args) > 1 else "auto"
        # Convert "auto" to None to trigger auto-detection
        chunk_strategy = None if strategy == "auto" else strategy

        self.display.print_info(f"Adding documents from: {source}")
        self.display.print_thinking()

        try:
            chunks = await self.agent.rag_add_documents(
                source=source,
                chunk_strategy=chunk_strategy
            )

            self.display.print_success(f"✅ Added {chunks} chunks to knowledge base")

        except Exception as e:
            self.display.print_error(f"Failed to add documents: {e}")

        return True

    async def _kb_list_command(self) -> bool:
        """Handle /kb list command.

        Usage: /kb list
        """
        self.display.print_thinking()

        try:
            stats = self.agent.get_rag_stats()
            collections = stats.get("collection_info", {})

            if not collections or collections.get("count", 0) == 0:
                self.display.print_info("Knowledge base is empty.")
                self.display.print_info("Use '/kb add <path>' to add documents.")
                return True

            self.display.print_info("")
            self.display.print_info("Knowledge Base Collections")
            self.display.print_info("============================")

            # List collection info
            collection_name = collections.get("name", "default")
            count = collections.get("count", 0)

            self.display.print_info(f"Collection: {collection_name}")
            self.display.print_info(f"Total chunks: {count}")

            # Show additional stats
            usage_stats = stats.get("usage_stats", {})
            if usage_stats.get("documents_loaded", 0) > 0:
                self.display.print_info(f"Source documents: {usage_stats['documents_loaded']}")

            if usage_stats.get("queries_processed", 0) > 0:
                self.display.print_info(f"Queries processed: {usage_stats['queries_processed']}")

        except Exception as e:
            self.display.print_error(f"Failed to list collections: {e}")

        return True

    async def _kb_stats_command(self) -> bool:
        """Handle /kb stats command.

        Usage: /kb stats
        """
        self.display.print_thinking()

        try:
            stats = self.agent.get_rag_stats()

            if not stats.get("initialized", False):
                self.display.print_info("RAG is not enabled.")
                return True

            self.display.print_info("")
            self.display.print_info("Knowledge Base Statistics")
            self.display.print_info("==========================")

            # RAG config
            config = stats.get("config", {})
            self.display.print_info(f"Embedder model: {config.get('embedder_model', 'N/A')}")
            self.display.print_info(f"Chunk size: {config.get('chunk_size', 'N/A')}")
            self.display.print_info(f"Top K: {config.get('top_k', 'N/A')}")

            # Usage stats
            usage = stats.get("usage_stats", {})
            self.display.print_info(f"Documents loaded: {usage.get('documents_loaded', 0)}")
            self.display.print_info(f"Chunks created: {usage.get('chunks_created', 0)}")
            self.display.print_info(f"Queries processed: {usage.get('queries_processed', 0)}")

            # Collection info
            collection = stats.get("collection_info", {})
            if collection:
                self.display.print_info(f"Collection: {collection.get('name', 'N/A')}")
                self.display.print_info(f"Total chunks: {collection.get('count', 0)}")

        except Exception as e:
            self.display.print_error(f"Failed to get stats: {e}")

        return True

    async def _kb_search_command(self, args: list[str]) -> bool:
        """Handle /kb search command.

        Usage: /kb search <query> [top_k]
        """
        if not args:
            self.display.print_error("Usage: /kb search <query> [top_k]")
            return True

        query = " ".join(args[:-1]) if len(args) > 1 else args[0]
        top_k = int(args[-1]) if args[-1].isdigit() else 3

        self.display.print_info(f"Searching knowledge base for: {query}")
        self.display.print_thinking()

        try:
            result = await self.agent.rag_query(query, top_k=top_k)

            # Display answer
            self.display.print_info("")
            self.display.print_info("Search Results")
            self.display.print_info("==============")
            self.display.print_message(result.answer)

            # Display sources
            if result.has_sources:
                self.display.print_info("\nSources:")
                for i, source in enumerate(result.sources, 1):
                    score = source.score
                    content = source.document.content[:100] + "..." if len(source.document.content) > 100 else source.document.content
                    source_name = source.document.metadata.get("source", "Unknown")
                    self.display.print_info(f"{i}. [{source_name}] (relevance: {score:.2f})")
                    self.display.print_info(f"   {content}")
            else:
                self.display.print_warning("\nNo relevant information found.")

        except Exception as e:
            self.display.print_error(f"Search failed: {e}")

        return True

    async def _kb_clear_command(self) -> bool:
        """Handle /kb clear command.

        Usage: /kb clear
        """
        self.display.print_warning("This will delete ALL documents from the knowledge base!")

        # In a real implementation, you'd ask for confirmation
        # For now, just do it
        try:
            await self.agent.rag_clear()
            self.display.print_success("Knowledge base cleared.")

        except Exception as e:
            self.display.print_error(f"Failed to clear knowledge base: {e}")

        return True

    async def _kb_delete_command(self, args: list[str]) -> bool:
        """Handle /kb delete command.

        Usage: /kb delete <collection_name>
        """
        if not args:
            self.display.print_error("Usage: /kb delete <collection_name>")
            return True

        collection_name = args[0]

        try:
            # This would require extending the RAG engine
            self.display.print_info(f"Delete collection '{collection_name}' (not yet implemented)")
            # await self.agent.rag_delete_collection(collection_name)

        except Exception as e:
            self.display.print_error(f"Failed to delete collection: {e}")

        return True

    def get_prompt(self) -> str:
        """Get the appropriate input prompt.

        Returns:
            Prompt string
        """
        if self.multiline_mode:
            return "[dim]... [/dim]"
        return "[cyan]> [/cyan] "