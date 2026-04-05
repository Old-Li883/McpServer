#!/usr/bin/env python3
"""Basic usage example for MCP Agent.

This example shows how to use the MCP Agent programmatically.
"""

import asyncio
from pathlib import Path

# Add parent directory to path for imports
import sys
sys.path.insert(0, str(Path(__file__).parent.parent))

from agent.config import Config
from agent.core.agent_engine import create_agent


async def main():
    """Main example function."""
    # Create configuration (or use defaults)
    config = Config(
        mcp={"server_url": "http://localhost:8080"},
        llm={"base_url": "http://localhost:11434", "model": "llama3.2"},
    )

    # Create and initialize the agent
    agent = await create_agent(config)

    # Show available tools
    tools = agent.get_available_tools()
    print(f"Available tools: {tools}")

    # Process a message
    response = await agent.process_message("What tools do you have available?")
    print(f"Response: {response}")

    # Process another message
    response = await agent.process_message("Tell me a joke.")
    print(f"Response: {response}")

    # Get conversation history
    history = agent.get_conversation_history()
    print(f"Conversation has {len(history)} messages")


if __name__ == "__main__":
    asyncio.run(main())
