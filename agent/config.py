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


class RagConfig(BaseModel):
    """RAG (Retrieval-Augmented Generation) configuration."""

    enabled: bool = False
    always_enabled: bool = False  # Always use RAG for all queries (ignores keyword detection)
    embedder_model: str = "nomic-embed-text"
    embedder_base_url: str = "http://localhost:11434"
    vector_db_path: str = "./data/chroma"
    collection_name: str = "knowledge_base"
    chunk_size: int = 512
    chunk_overlap: int = 50
    top_k: int = 5
    score_threshold: float = 0.5
    enable_cache: bool = True
    cache_path: str = "./data/embeddings_cache"
    enable_chinese_optimization: bool = True


class MemoryConfig(BaseModel):
    """Memory system configuration."""

    # Short-term memory configuration
    short_term_max_messages: int = 100
    short_term_summary_threshold: int = 80
    short_term_importance_threshold: float = 0.6

    # Long-term memory configuration
    long_term_enabled: bool = True
    long_term_vector_db_path: str = "./data/memory_chroma"
    long_term_collection_name: str = "long_term_memory"
    long_term_embedder_model: str = "nomic-embed-text"
    long_term_embedder_base_url: str = "http://localhost:11434"

    # Auto-save configuration
    auto_save_to_long_term: bool = True
    auto_save_importance_threshold: float = 0.7

    # Retrieval configuration
    retrieval_top_k: int = 5
    retrieval_min_score: float = 0.5


class HRConfig(BaseModel):
    """HR module configuration."""

    enabled: bool = True
    llm_model: str = "qwen2.5:7b"
    db_path: str = "./data/hr.db"
    chroma_collection: str = "hr_resumes"
    parse_retries: int = 1
    top_k_default: int = 5
    score_weights: dict[str, float] = Field(
        default_factory=lambda: {"education": 0.4, "skills": 0.6}
    )


class Config(BaseModel):
    """Main configuration."""

    agent: AgentConfig = Field(default_factory=AgentConfig)
    mcp: McpConfig = Field(default_factory=McpConfig)
    llm: LlmConfig = Field(default_factory=LlmConfig)
    conversation: ConversationConfig = Field(default_factory=ConversationConfig)
    cli: CliConfig = Field(default_factory=CliConfig)
    rag: RagConfig = Field(default_factory=RagConfig)
    memory: MemoryConfig = Field(default_factory=MemoryConfig)
    hr: HRConfig = Field(default_factory=HRConfig)

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
