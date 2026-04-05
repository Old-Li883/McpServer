"""CLI command handling for the MCP Agent."""

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

        else:
            self.display.print_error(f"Unknown command: {input_text}")
            self.display.print_info("Type 'help' for available commands.")

        return True

    def get_prompt(self) -> str:
        """Get the appropriate input prompt.

        Returns:
            Prompt string
        """
        if self.multiline_mode:
            return "[dim]... [/dim]"
        return "[cyan]>[/cyan] "
