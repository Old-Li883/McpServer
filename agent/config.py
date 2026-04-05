"""Configuration management for MCP Agent."""

from pathlib import Path
from typing import Optional

import yaml
from pydantic import BaseModel, Field


class AgentConfig(BaseModel):
    """Agent configuration."""

    name: str = "mcp-agent"
    log_level: str = "INFO"


class McpConfig(BaseModel):
    """MCP server configuration."""

    server_url: str = "http://localhost:8080"
    timeout: int = 30


class LlmConfig(BaseModel):
    """LLM configuration."""

    base_url: str = "http://localhost:11434"
    model: str = "llama3.2"
    temperature: float = 0.7
    max_tokens: int = 2048


class ConversationConfig(BaseModel):
    """Conversation configuration."""

    max_history: int = 100
    system_prompt: str = """You are a helpful AI assistant with access to tools and resources.
You can help users by:
- Answering questions using your knowledge
- Calling available tools when needed
- Reading and processing resources
- Providing clear and concise responses"""


class CliConfig(BaseModel):
    """CLI configuration."""

    prompt_symbol: str = "> "
    multiline_prompt_symbol: str = "... "
    show_thinking: bool = True


class Config(BaseModel):
    """Main configuration."""

    agent: AgentConfig = Field(default_factory=AgentConfig)
    mcp: McpConfig = Field(default_factory=McpConfig)
    llm: LlmConfig = Field(default_factory=LlmConfig)
    conversation: ConversationConfig = Field(default_factory=ConversationConfig)
    cli: CliConfig = Field(default_factory=CliConfig)

    @classmethod
    def from_yaml(cls, path: Path) -> "Config":
        """Load configuration from YAML file."""
        with open(path, "r") as f:
            data = yaml.safe_load(f) or {}
        return cls(**data)

    def to_yaml(self, path: Path) -> None:
        """Save configuration to YAML file."""
        with open(path, "w") as f:
            yaml.dump(self.model_dump(exclude_none=True), f, default_flow_style=False)


def load_config(path: Optional[Path] = None) -> Config:
    """Load configuration from file or use defaults."""
    if path is None:
        # Default config file location
        module_dir = Path(__file__).parent
        path = module_dir / "config.yaml"

    if path.exists():
        return Config.from_yaml(path)
    return Config()
