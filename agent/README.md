# MCP Agent

Python Agent for C++ MCP Server with local LLM integration.

## Overview

The MCP Agent is a Python-based agent that connects to a C++ MCP (Model Context Protocol) server and provides intelligent question-answering capabilities using local LLM models through Ollama.

## Features

- **MCP Protocol Client**: Full support for Tools, Resources, and Prompts
- **LLM Integration**: Uses Ollama for local LLM inference
- **Interactive CLI**: Rich terminal interface with syntax highlighting
- **Tool Calling**: Automatic tool discovery and execution
- **Conversation Memory**: Maintains conversation context

## Installation

```bash
# Install dependencies
pip install -e agent/

# Or install manually
pip install httpx pydantic pyyaml ollama rich prompt-toolkit loguru
```

## Configuration

Edit `agent/config.yaml` to customize:

```yaml
agent:
  name: "mcp-agent"
  log_level: "INFO"

mcp:
  server_url: "http://localhost:8080"
  timeout: 30

llm:
  base_url: "http://localhost:11434"
  model: "llama3.2"
  temperature: 0.7
  max_tokens: 2048
```

## Usage

### Interactive Mode

```bash
python -m agent.cli.main
```

### Single Message Mode

```bash
python -m agent.cli.main -m "What tools do you have?"
```

### Custom Configuration

```bash
python -m agent.cli.main --config custom-config.yaml
python -m agent.cli.main --mcp-url http://localhost:9090 --llm-url http://localhost:11434
```

## CLI Commands

- `help` - Show available commands
- `clear` - Clear conversation history
- `tools` - List available tools
- `caps` - Show agent capabilities
- `multiline` - Toggle multiline input mode
- `quit` or `exit` - Exit the agent

## Architecture

```
┌─────────────────────────────────────────┐
│           CLI Interface                 │
│       (agent/cli/main.py)               │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│          Agent Engine                   │
│     (agent/core/agent_engine.py)        │
│  - Conversation Manager                 │
│  - Tool Orchestrator                    │
└────────┬──────────────────┬─────────────┘
         │                  │
┌────────▼────────┐   ┌────▼──────────────────┐
│   LLM Client    │   │   MCP Client          │
│  (Ollama)       │   │  (HTTP JSON-RPC)      │
└─────────────────┘   └────┬──────────────────┘
                           │
                  ┌────────▼─────────┐
                  │  C++ MCP Server  │
                  │  (localhost:8080)│
                  └──────────────────┘
```

## Project Structure

```
agent/
├── __init__.py
├── config.py              # Configuration management
├── config.yaml            # Default configuration
├── pyproject.toml         # Python project config
├── mcp/
│   ├── __init__.py
│   ├── types.py          # MCP protocol data structures
│   └── client.py         # HTTP JSON-RPC client
├── llm/
│   ├── __init__.py
│   ├── ollama_client.py  # Ollama integration
│   ├── prompt_builder.py # Prompt construction
│   └── response_parser.py # Response parsing
├── core/
│   ├── __init__.py
│   ├── agent_engine.py   # Core orchestration
│   ├── conversation.py   # Message history
│   └── tools.py          # Tool orchestration
└── cli/
    ├── __init__.py
    ├── main.py           # CLI entry point
    ├── display.py        # Rich display
    └── commands.py       # Command handling
```

## Development

### Running Tests

```bash
pip install -e agent/[dev]
pytest agent/tests/
```

### Code Formatting

```bash
black agent/
ruff check agent/
mypy agent/
```

## Requirements

- Python 3.10+
- C++ MCP Server running on port 8080
- Ollama running on port 11434

## License

MIT
