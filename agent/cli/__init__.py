"""CLI module for MCP Agent."""

from agent.cli.commands import CLICommands
from agent.cli.display import Display
from agent.cli.main import main

__all__ = ["main", "CLICommands", "Display"]
