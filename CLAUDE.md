# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MCP Server is a two-part system implementing the Model Context Protocol (MCP):
1. **C++ MCP Server** — the core server handling MCP protocol (Tools, Resources, Prompts) via JSON-RPC 2.0 over HTTP or stdio
2. **Python Agent** (`agent/`) — an AI agent that connects to the C++ server, using local LLM (Ollama) for inference with RAG and memory support

Namespace: `mcpserver::mcp` (C++ server), `mcpserver::json_rpc` (JSON-RPC layer).


## Architecture

### C++ Server Layer (bottom-up)

```
main.cpp
  └─ McpServerRunner  (src/mcp/McpServerRunner.{h,cpp})
       ├─ JsonRpc           (src/json_rpc/jsonrpc.{h,cpp})        — JSON-RPC 2.0 processor
       ├─ McpServer         (src/mcp/McpServer.{h,cpp})          — MCP protocol handler
       │    ├─ ToolManager       (src/mcp/ToolManager.{h,cpp})
       │    ├─ ResourceManager   (src/mcp/ResourceManager.{h,cpp})
       │    └─ PromptManager     (src/mcp/PromptManager.{h,cpp})
       ├─ HttpJsonRpcServer (src/json_rpc/http_jsonrpc_server.{h,cpp}) — HTTP transport via cpp-httplib
       └─ StdioJsonRpc      (src/json_rpc/stdio_jsonrpc.{h,cpp})      — stdio transport (LSP frame format)
```

- **McpServerRunner** is the unified entry point. It wires together the JSON-RPC processor, MCP handler, and transport layers. Supports HTTP, stdio, or both modes.
- **McpServer** registers MCP protocol methods (`initialize`, `tools/*`, `resources/*`, `prompts/*`) onto `JsonRpc` via `registerMethods()`.
- **JsonRpc** handles request parsing, method dispatch, batch requests, and notifications. Thread-safe.
- All MCP types (Tool, Resource, Prompt, ToolResult, etc.) are defined in `src/mcp/types.h` with `to_json()`/`from_json()` serialization.
- Transport layers: `HttpJsonRpcServer` (cpp-httplib) and `StdioJsonRpc` (Content-Length header framing).
- Configuration via `config/Config.{h,cpp}`, loaded from `config.json`.
- Logging via `src/logger/Logger.{h,cpp}` using spdlog macros in `LogMacros.h`.

### Python Agent (top-down)

```
agent/cli/main.py          — CLI entry point (prompt-toolkit + rich)
  └─ core/agent_engine.py  — Orchestrates LLM calls, MCP tool execution, RAG, memory
       ├─ mcp/client.py          — HTTP JSON-RPC client to C++ server
       ├─ llm/ollama_client.py   — Ollama LLM integration
       ├─ llm/prompt_builder.py  — System prompt + tool definitions
       ├─ llm/response_parser.py — Parse LLM output for tool calls
       ├─ rag/                   — RAG pipeline (document loading, embedding, ChromaDB storage, retrieval)
       └─ memory/                — Short-term (conversation history) and long-term (vector DB) memory
```

- The agent uses a ReAct-style loop: LLM decides actions → agent executes tools/resources via MCP → results fed back to LLM.
- RAG system: document loaders (PDF, HTML, DOCX, TXT), text processors, embeddings (Ollama nomic-embed-text), ChromaDB vector store, retrieval with relevance scoring.
- Memory: short-term (conversation buffer with summarization) + long-term (persistent vector DB for cross-session recall).

## Coding Conventions

### C++ Conventions

- **Standard**: C++17, compiled with `-std=c++17`
- **Namespaces**: `mcpserver::mcp` for MCP protocol layer, `mcpserver::json_rpc` for JSON-RPC layer
- **Header guards**: Use `#pragma once`
- **Naming**:
  - Classes/Structs: PascalCase (`McpServer`, `ToolInputSchema`)
  - Functions/Methods: camelCase (`registerTool`, `handleRequest`)
  - Member variables: snake_case with trailing underscore (`config_`, `tool_manager_`, `mutex_`)
  - Local variables: snake_case (`request_id`, `error_message`)
  - Constants/Enums: PascalCase enum class (`ServerMode::HTTP`, `ContentType::TEXT`)
  - Files: PascalCase matching the class name (`McpServer.h`/`McpServer.cpp`, `ToolManager.h`/`ToolManager.cpp`)
- **Class design**:
  - Disable copy and move for service classes (`= delete`)
  - Use `[[nodiscard]]` on all non-void methods that return values
  - Use `std::unique_ptr` for owned dependencies, pass by reference for non-owned
  - Private members go at the bottom of the class declaration, public interface at the top
  - Use `// ========== Section Headers ==========` comments to organize method groups in headers
- **Thread safety**: Use `std::mutex` with `std::lock_guard` for shared state. Release locks before calling user-provided callbacks to avoid deadlocks (see `ToolManager::callTool`)
- **JSON serialization**: All MCP types provide `to_json()` / `from_json()` methods. Use `nlohmann::json` throughout
- **Logging**: Use `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_DEBUG` macros from `LogMacros.h` (spdlog-based). Use fmt-style formatting: `LOG_INFO("Tool registered: {}", name)`
- **Error handling in handlers**: Wrap tool handler calls in try/catch. Return `ToolResult::error(...)` for expected failures, let exceptions propagate for unexpected ones
- **Comments**: Doxygen-style (`/** ... */`) for public API in headers. Brief one-liners for parameters. No comments in .cpp files unless the WHY is non-obvious
- **Includes**: Project headers use relative paths (`#include "../logger/Logger.h"`). System/library headers use angle brackets (`<string>`, `<nlohmann/json.hpp>`)
- **Convenience overloads**: Provide both a "full type" overload and a "parameter list" overload (e.g., `registerTool(Tool, Handler)` and `registerTool(string, string, Schema, Handler)`)

### Python Conventions

- **Python version**: 3.10+ (use `X | Y` union syntax, `list[str]` generics without quotes)
- **Formatting**: `black` (line length 100), `ruff` for linting (`pyproject.toml` config)
- **Type hints**: Required on all function signatures. Use `Optional[X]` for nullable, `dict[str, Any]` for generic dicts
- **Data models**: Use Pydantic `BaseModel` for all structured data (`Tool`, `Resource`, `Prompt`, etc.). Provide `to_dict()` and `from_dict(cls)` methods mirroring the C++ `to_json()`/`from_json()` pattern
- **Naming**:
  - Classes: PascalCase (`McpClient`, `AgentEngine`, `ToolResult`)
  - Functions/methods: snake_case (`process_message`, `list_tools`)
  - Private attributes: leading underscore (`_rag_engine`, `_memory_enabled`, `_request_id`)
  - Constants: UPPER_SNAKE_CASE
  - Files: snake_case (`agent_engine.py`, `ollama_client.py`)
- **Async**: All I/O-bound methods are `async`. Use `asyncio` throughout. Clients implement async context manager (`__aenter__`/`__aexit__`)
- **Docstrings**: Google-style docstrings on all public classes and methods with `Args:`, `Returns:`, `Raises:` sections
- **Imports**: Absolute imports from package root (`from agent.mcp.types import Tool`). Optional dependencies guarded with try/except ImportError (`RAG_AVAILABLE`, `MEMORY_AVAILABLE` flags)
- **Error handling**: Define a custom exception hierarchy per module (`McpClientError` → `McpConnectionError`, `McpRpcError`). Chain exceptions with `from e`
- **Config**: Use Pydantic models loaded from `config.yaml` via `agent/config.py`
- **Logging**: Use `loguru` or plain `print()` for agent-level messages (not Python `logging` module)

### Cross-language Consistency

- Python types in `agent/mcp/types.py` mirror C++ types in `src/mcp/types.h` — keep them in sync when modifying MCP protocol types
- JSON field names follow camelCase in protocol (e.g., `inputSchema`, `mimeType`, `isError`), matching the MCP specification
- Both sides use the same MCP method names: `initialize`, `tools/list`, `tools/call`, `resources/list`, `resources/read`, `prompts/list`, `prompts/get`

## Key Patterns

### Adding a new MCP Tool (C++ side)

In `main.cpp` (or wherever tools are registered), call `runner.registerTool()` with a name, description, `ToolInputSchema`, and a `ToolHandler` lambda:

```cpp
runner.registerTool("my_tool", "Description",
    ToolInputSchema{{"type", "object"}, {{"properties", {...}}, {"required", {...}}}},
    [](const std::string& name, const nlohmann::json& args) -> ToolResult {
        return ToolResult::success({ContentItem::text_content("result")});
    });
```

### C++ Dependencies

C++17, macOS (Apple Silicon). Uses Homebrew libs from `/opt/homebrew`. CMake explicitly filters out miniconda3 paths to avoid conflicts. spdlog and fmt are statically linked.

### Config

- C++ server: `config.json` (port, mode, logging)
- Python agent: `agent/config.yaml` (MCP server URL, Ollama settings, RAG config, memory config, system prompt)

