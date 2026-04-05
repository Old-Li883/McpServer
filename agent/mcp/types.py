"""MCP protocol data structures.

These structures mirror the C++ types defined in src/mcp/types.h
"""

from enum import Enum
from typing import Any, Optional

from pydantic import BaseModel, Field


# ========== Tool related data structures ==========


class ToolInputSchema(BaseModel):
    """Tool input schema (JSON Schema format)."""

    type: str = "object"
    properties: dict[str, Any] = Field(default_factory=dict)
    required: list[str] = Field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {"type": self.type}
        if self.properties:
            result["properties"] = self.properties
        if self.required:
            result["required"] = self.required
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ToolInputSchema":
        """Create from dictionary."""
        return cls(
            type=data.get("type", "object"),
            properties=data.get("properties", {}),
            required=data.get("required", []),
        )


class ContentType(str, Enum):
    """Content item types."""

    TEXT = "text"
    IMAGE = "image"
    RESOURCE = "resource"


class ContentItem(BaseModel):
    """Content item for tool results."""

    type: str
    text: Optional[str] = None
    data: Optional[str] = None  # base64 encoded data (for images)
    mime_type: Optional[str] = None
    uri: Optional[str] = None

    @classmethod
    def text_content(cls, content: str) -> "ContentItem":
        """Create a text content item."""
        return cls(type="text", text=content)

    @classmethod
    def image_content(cls, base64_data: str, mime: str = "image/png") -> "ContentItem":
        """Create an image content item."""
        return cls(type="image", data=base64_data, mime_type=mime)

    @classmethod
    def resource_content(cls, resource_uri: str) -> "ContentItem":
        """Create a resource content item."""
        return cls(type="resource", uri=resource_uri)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {"type": self.type}
        if self.text is not None:
            result["text"] = self.text
        if self.data is not None:
            result["data"] = self.data
        if self.mime_type is not None:
            result["mime_type"] = self.mime_type
        if self.uri is not None:
            result["uri"] = self.uri
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ContentItem":
        """Create from dictionary."""
        return cls(
            type=data["type"],
            text=data.get("text"),
            data=data.get("data"),
            mime_type=data.get("mime_type"),
            uri=data.get("uri"),
        )


class ToolResult(BaseModel):
    """Tool call result."""

    content: list[ContentItem] = Field(default_factory=list)
    is_error: bool = False
    _error_message: Optional[str] = None

    @classmethod
    def success(cls, items: list[ContentItem]) -> "ToolResult":
        """Create a successful result."""
        return cls(content=items, is_error=False)

    @classmethod
    def error(cls, message: str) -> "ToolResult":
        """Create an error result."""
        return cls(
            content=[ContentItem.text_content(message)],
            is_error=True,
            _error_message=message,
        )

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        content_array = [item.to_dict() for item in self.content]
        result: dict[str, Any] = {"content": content_array}
        if self.is_error:
            result["isError"] = True
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ToolResult":
        """Create from dictionary."""
        items = []
        if "content" in data and isinstance(data["content"], list):
            items = [ContentItem.from_dict(item) for item in data["content"]]

        is_error = data.get("isError", False)
        return cls(content=items, is_error=is_error, _error_message=data.get("errorMessage"))


class Tool(BaseModel):
    """Tool definition."""

    name: str
    description: str
    input_schema: ToolInputSchema = Field(default_factory=ToolInputSchema)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {
            "name": self.name,
            "description": self.description,
            "inputSchema": self.input_schema.to_dict(),
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Tool":
        """Create from dictionary."""
        return cls(
            name=data["name"],
            description=data["description"],
            input_schema=ToolInputSchema.from_dict(data.get("inputSchema", {})),
        )


# ========== Resource related data structures ==========


class Resource(BaseModel):
    """Resource definition."""

    uri: str
    name: str
    description: Optional[str] = None
    mime_type: Optional[str] = None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {"uri": self.uri, "name": self.name}
        if self.description is not None:
            result["description"] = self.description
        if self.mime_type is not None:
            result["mimeType"] = self.mime_type
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Resource":
        """Create from dictionary."""
        return cls(
            uri=data["uri"],
            name=data["name"],
            description=data.get("description"),
            mime_type=data.get("mimeType"),
        )


class ResourceContent(BaseModel):
    """Resource content."""

    uri: str
    mime_type: Optional[str] = None
    text: str = ""
    blob: Optional[str] = None  # base64 encoded binary content

    @classmethod
    def text_resource(cls, resource_uri: str, content: str) -> "ResourceContent":
        """Create a text resource content."""
        return cls(uri=resource_uri, text=content)

    @classmethod
    def blob_resource(cls, resource_uri: str, base64_data: str, mime: str = "application/octet-stream") -> "ResourceContent":
        """Create a binary resource content."""
        return cls(uri=resource_uri, blob=base64_data, mime_type=mime)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {"uri": self.uri}
        if self.mime_type is not None:
            result["mimeType"] = self.mime_type
        if self.blob is not None:
            result["blob"] = self.blob
        else:
            result["text"] = self.text
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ResourceContent":
        """Create from dictionary."""
        return cls(
            uri=data["uri"],
            mime_type=data.get("mimeType"),
            text=data.get("text", ""),
            blob=data.get("blob"),
        )


# ========== Prompt related data structures ==========


class Role(str, Enum):
    """Role types for messages."""

    USER = "user"
    ASSISTANT = "assistant"
    SYSTEM = "system"


class PromptArgument(BaseModel):
    """Prompt argument definition."""

    name: str
    description: Optional[str] = None
    required: bool = False

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {"name": self.name}
        if self.description is not None:
            result["description"] = self.description
        if self.required:
            result["required"] = True
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "PromptArgument":
        """Create from dictionary."""
        return cls(
            name=data["name"],
            description=data.get("description"),
            required=data.get("required", False),
        )


class PromptMessage(BaseModel):
    """Prompt message."""

    role: Role
    content: Any  # Can be string or object

    @classmethod
    def user_text(cls, text: str) -> "PromptMessage":
        """Create a user text message."""
        return cls(role=Role.USER, content=text)

    @classmethod
    def assistant_text(cls, text: str) -> "PromptMessage":
        """Create an assistant text message."""
        return cls(role=Role.ASSISTANT, content=text)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {"role": self.role.value, "content": self.content}

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "PromptMessage":
        """Create from dictionary."""
        return cls(role=Role(data["role"]), content=data["content"])


class PromptResult(BaseModel):
    """Prompt result."""

    messages: list[PromptMessage] = Field(default_factory=list)
    _error: Optional[str] = None

    @classmethod
    def success(cls, messages: list[PromptMessage]) -> "PromptResult":
        """Create a successful result."""
        return cls(messages=messages)

    @classmethod
    def error(cls, message: str) -> "PromptResult":
        """Create an error result."""
        return cls(_error=message)

    def has_error(self) -> bool:
        """Check if there's an error."""
        return self._error is not None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        if self._error is not None:
            return {"error": self._error}
        return {"messages": [msg.to_dict() for msg in self.messages]}

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "PromptResult":
        """Create from dictionary."""
        if "error" in data:
            return cls(_error=data["error"])
        messages = [PromptMessage.from_dict(msg) for msg in data.get("messages", [])]
        return cls(messages=messages)


class Prompt(BaseModel):
    """Prompt definition."""

    name: str
    description: Optional[str] = None
    arguments: list[PromptArgument] = Field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {"name": self.name}
        if self.description is not None:
            result["description"] = self.description
        if self.arguments:
            result["arguments"] = [arg.to_dict() for arg in self.arguments]
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Prompt":
        """Create from dictionary."""
        arguments = []
        if "arguments" in data and isinstance(data["arguments"], list):
            arguments = [PromptArgument.from_dict(arg) for arg in data["arguments"]]
        return cls(
            name=data["name"],
            description=data.get("description"),
            arguments=arguments,
        )


# ========== Server capabilities ==========


class ServerCapabilities(BaseModel):
    """MCP server capabilities."""

    tools: bool = False
    resources: bool = False
    prompts: bool = False

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        result: dict[str, Any] = {}
        if self.tools:
            result["tools"] = {}
        if self.resources:
            result["resources"] = {}
        if self.prompts:
            result["prompts"] = {}
        return result

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "ServerCapabilities":
        """Create from dictionary."""
        return cls(
            tools="tools" in data,
            resources="resources" in data,
            prompts="prompts" in data,
        )


class InitializeResult(BaseModel):
    """Initialize result from server."""

    capabilities: ServerCapabilities
    server_info: str
    version: str = "1.0.0"

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "InitializeResult":
        """Create from dictionary."""
        server_info_data = data.get("serverInfo", {})
        return cls(
            capabilities=ServerCapabilities.from_dict(data.get("capabilities", {})),
            server_info=server_info_data.get("name", "unknown"),
            version=server_info_data.get("version", "1.0.0"),
        )
