"""MCP Agent - Python client for C++ MCP Server."""

__version__ = "0.1.0"

from agent.config import Config, load_config

__all__ = ["Config", "load_config", "__version__"]
