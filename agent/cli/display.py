"""Display utilities for the MCP Agent CLI."""

from enum import Enum
from typing import Any, Optional

from rich.console import Console
from rich.markdown import Markdown
from rich.panel import Panel
from rich.syntax import Syntax
from rich.table import Table
from rich.text import Text


class MessageRole(str, Enum):
    """Message role types for display."""

    USER = "user"
    ASSISTANT = "assistant"
    SYSTEM = "system"
    TOOL = "tool"
    ERROR = "error"
    INFO = "info"


class Display:
    """Rich terminal display for the MCP Agent CLI."""

    # Color scheme for different message types
    COLORS = {
        MessageRole.USER: "cyan",
        MessageRole.ASSISTANT: "green",
        MessageRole.SYSTEM: "yellow",
        MessageRole.TOOL: "blue",
        MessageRole.ERROR: "red",
        MessageRole.INFO: "dim",
    }

    def __init__(self, console: Optional[Console] = None):
        """Initialize the display.

        Args:
            console: Optional rich Console (creates new one if not provided)
        """
        self.console = console or Console()

    def print_message(
        self,
        content: str,
        role: MessageRole = MessageRole.ASSISTANT,
        title: Optional[str] = None,
    ) -> None:
        """Print a message with appropriate styling.

        Args:
            content: Message content
            role: Message role type
            title: Optional title for the message
        """
        color = self.COLORS.get(role, "white")

        if role == MessageRole.USER:
            title = title or "You"
            self.console.print(Panel(content, title=title, border_style=color))
        elif role == MessageRole.ASSISTANT:
            # Render as markdown for assistant messages
            md = Markdown(content)
            self.console.print(md)
        elif role == MessageRole.ERROR:
            title = title or "Error"
            self.console.print(Panel(content, title=title, border_style="red"))
        elif role == MessageRole.TOOL:
            title = title or "Tool Call"
            self.console.print(Panel(content, title=title, border_style=color))
        else:
            if title:
                self.console.print(Panel(content, title=title, border_style=color))
            else:
                self.console.print(content, style=color)

    def print_info(self, message: str) -> None:
        """Print an info message.

        Args:
            message: Info message
        """
        self.console.print(f"[dim]{message}[/dim]")

    def print_error(self, message: str) -> None:
        """Print an error message.

        Args:
            message: Error message
        """
        self.console.print(f"[red]Error: {message}[/red]")

    def print_warning(self, message: str) -> None:
        """Print a warning message.

        Args:
            message: Warning message
        """
        self.console.print(f"[yellow]Warning: {message}[/yellow]")

    def print_success(self, message: str) -> None:
        """Print a success message.

        Args:
            message: Success message
        """
        self.console.print(f"[green]{message}[/green]")

    def print_code(
        self,
        code: str,
        language: str = "python",
        title: Optional[str] = None,
    ) -> None:
        """Print a code block with syntax highlighting.

        Args:
            code: Code to display
            language: Programming language
            title: Optional title for the code block
        """
        syntax = Syntax(code, language, theme="monokai", line_numbers=True)
        if title:
            self.console.print(Panel(syntax, title=title, border_style="blue"))
        else:
            self.console.print(syntax)

    def print_table(
        self,
        data: list[dict[str, Any]],
        title: Optional[str] = None,
    ) -> None:
        """Print data as a table.

        Args:
            data: List of dictionaries with consistent keys
            title: Optional table title
        """
        if not data:
            self.print_info("No data to display")
            return

        table = Table(title=title)
        table.add_column("Key", style="cyan")
        table.add_column("Value", style="green")

        for item in data:
            for key, value in item.items():
                table.add_row(str(key), str(value))

        self.console.print(table)

    def print_tool_list(self, tools: list[str]) -> None:
        """Print a list of available tools.

        Args:
            tools: List of tool names
        """
        if not tools:
            self.print_info("No tools available")
            return

        table = Table(title="Available Tools")
        table.add_column("No.", style="dim", width=3)
        table.add_column("Tool Name", style="cyan")

        for i, tool in enumerate(tools, 1):
            table.add_row(str(i), tool)

        self.console.print(table)

    def print_capabilities(self, capabilities: dict[str, Any]) -> None:
        """Print agent capabilities.

        Args:
            capabilities: Capabilities dictionary
        """
        table = Table(title="Agent Capabilities")
        table.add_column("Capability", style="cyan")
        table.add_column("Details", style="green")

        # LLM Model
        table.add_row("LLM Model", capabilities.get("llm_model", "unknown"))

        # Tools
        tools = capabilities.get("tools", [])
        tool_count = len(tools) if isinstance(tools, list) else 0
        table.add_row("Available Tools", str(tool_count))

        # MCP Server
        mcp = capabilities.get("mcp_server", {})
        if isinstance(mcp, dict):
            features = []
            if mcp.get("tools"):
                features.append("Tools")
            if mcp.get("resources"):
                features.append("Resources")
            if mcp.get("prompts"):
                features.append("Prompts")
            table.add_row("MCP Server Features", ", ".join(features) if features else "None")

        self.console.print(table)

    def print_welcome(self, version: str = "0.1.0") -> None:
        """Print welcome message.

        Args:
            version: Agent version
        """
        welcome = f"""
[bold cyan]MCP Agent v{version}[/bold cyan]

A Python agent for the C++ MCP Server with local LLM integration.

Type [bold]help[/bold] for available commands.
Type [bold]quit[/bold] or [bold]exit[/bold] to quit.
"""
        self.console.print(Panel(welcome, border_style="cyan"))

    def print_help(self) -> None:
        """Print help information."""
        help_text = """
[bold cyan]Available Commands:[/bold cyan]

  [yellow]help[/yellow]       - Show this help message
  [yellow]clear[/yellow]      - Clear conversation history
  [yellow]tools[/yellow]      - List available tools
  [yellow]caps[/yellow]       - Show agent capabilities
  [yellow]quit[/yellow]       - Exit the agent

  [yellow]/multiline[/yellow] - Toggle multiline input mode
  [yellow]message[/yellow]    - Any other text is sent as a message
"""
        self.console.print(help_text)

    def print_thinking(self) -> None:
        """Print a thinking indicator."""
        self.console.print("[dim]Thinking...[/dim]")

    def clear_thinking(self) -> None:
        """Clear the thinking indicator (no-op for simple console)."""
        pass

    def input(self, prompt: str = "> ") -> str:
        """Get user input.

        Args:
            prompt: Input prompt

        Returns:
            User input string
        """
        return self.console.input(prompt)
