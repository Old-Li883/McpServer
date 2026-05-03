"""Main CLI entry point for the MCP Agent."""

import argparse
import asyncio
import sys
from pathlib import Path

from agent.cli.commands import CLICommands
from agent.cli.display import Display, MessageRole
from agent.config import Config, load_config
from agent.core.agent_engine import AgentEngine, create_agent
from agent.mcp.client import McpClientError
from agent.mcp.types import InitializeResult

# Agent version
__version__ = "0.1.0"


async def interactive_loop(agent: AgentEngine, config: Config) -> None:
    """Run the interactive CLI loop.

    Args:
        agent: Initialized agent engine
        config: Agent configuration
    """
    display = Display()
    commands = CLICommands(agent, display)

    # Print welcome message
    display.print_welcome(__version__)

    # Show capabilities on startup if configured
    if config.cli.show_thinking:
        caps = await agent.get_capabilities()
        display.print_capabilities(caps)

    # Main interaction loop
    while True:
        try:
            # Get user input
            prompt = commands.get_prompt()
            user_input = display.input(prompt)

            if not user_input.strip():
                continue

            # Handle multiline mode
            if commands.multiline_mode and user_input != ".":
                # Accumulate multiline input until user enters "."
                lines = [user_input]
                while True:
                    line = display.input("... ")
                    if line == ".":
                        break
                    lines.append(line)
                user_input = "\n".join(lines)

            # Check for special commands
            if commands.is_command(user_input):
                should_continue = await commands.execute(user_input)
                if not should_continue:
                    break
                continue

            # Process as a message
            display.print_thinking()

            try:
                response = await agent.process_message(user_input)

                # Display the response
                display.print_message(response, role=MessageRole.ASSISTANT)

            except Exception as e:
                display.print_error(f"Failed to process message: {e}")

        except KeyboardInterrupt:
            display.print_info("\nUse 'quit' or 'exit' to quit.")
        except EOFError:
            display.print_info("\nGoodbye!")
            break


async def main_async(args: argparse.Namespace) -> int:
    """Main async entry point.

    Args:
        args: Parsed command-line arguments

    Returns:
        Exit code (0 for success, non-zero for error)
    """
    # Load configuration
    config_path = Path(args.config) if args.config else None
    config = load_config(config_path) if config_path else load_config()

    # Override config with command-line options
    if args.mcp_url:
        config.mcp.server_url = args.mcp_url
    if args.llm_url:
        config.llm.base_url = args.llm_url
    if args.model:
        config.llm.model = args.model
    if args.verbose:
        config.agent.log_level = "DEBUG"

    display = Display()

    try:
        # Create and initialize agent
        display.print_info(f"Connecting to MCP server at {config.mcp.server_url}...")
        display.print_info(f"Using LLM at {config.llm.base_url} (model: {config.llm.model})...")

        agent = await create_agent(config)

        display.print_success("Connected!")

        # Run in interactive mode or single message mode
        if args.message:
            # Single message mode
            response = await agent.process_message(args.message)
            display.print_message(response, role=MessageRole.ASSISTANT)
            return 0
        else:
            # Interactive mode
            await interactive_loop(agent, config)
            return 0

    except McpClientError as e:
        display.print_error(f"Failed to connect to MCP server: {e}")
        display.print_info(f"Make sure the MCP server is running at {config.mcp.server_url}")
        return 1
    except Exception as e:
        display.print_error(f"An error occurred: {e}")
        if args.verbose:
            import traceback
            display.print_error(traceback.format_exc())
        return 1


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed arguments
    """
    parser = argparse.ArgumentParser(
        description="MCP Agent - Python client for C++ MCP Server with local LLM",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode (default)
  %(prog)s

  # Single message mode
  %(prog)s -m "Hello, what tools do you have?"

  # Use custom MCP server URL
  %(prog)s --mcp-url http://localhost:9090

  # Use custom Ollama URL and model
  %(prog)s --llm-url http://localhost:11434 --model llama3.2

  # Verbose mode
  %(prog)s -v
        """,
    )

    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output",
    )

    parser.add_argument(
        "-c", "--config",
        metavar="PATH",
        help="Path to configuration file (YAML)",
    )

    parser.add_argument(
        "-m", "--message",
        metavar="TEXT",
        help="Single message to process (non-interactive mode)",
    )

    parser.add_argument(
        "--mcp-url",
        metavar="URL",
        help="Override MCP server URL",
    )

    parser.add_argument(
        "--llm-url",
        metavar="URL",
        help="Override LLM (Ollama) URL",
    )

    parser.add_argument(
        "--model",
        metavar="NAME",
        help="Override LLM model name",
    )

    parser.add_argument(
        "--version",
        action="version",
        version=f"%(prog)s {__version__}",
    )

    return parser.parse_args()


def main() -> int:
    """Main entry point.

    Returns:
        Exit code
    """
    args = parse_args()

    try:
        # Run async main
        return asyncio.run(main_async(args))
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 130


if __name__ == "__main__":
    sys.exit(main())
