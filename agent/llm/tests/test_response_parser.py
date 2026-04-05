"""Unit tests for the ResponseParser module."""

import pytest

from agent.llm.response_parser import ResponseParser, ToolCall


def test_single_tool_call():
    """Test parsing a single tool call in JSON format."""
    parser = ResponseParser()
    response = """I'll check the time for you.

```json
{"tool": "get_current_time", "arguments": {}}
```

Let me know if you need anything else."""
    parsed = parser.parse(response)
    assert parsed.has_tool_calls
    assert len(parsed.tool_calls) == 1
    assert parsed.tool_calls[0].name == "get_current_time"
    assert parsed.tool_calls[0].arguments == {}
    assert "I'll check the time" in parsed.text
    assert "Let me know" in parsed.text


def test_multiple_tool_calls():
    """Test parsing multiple tool calls in JSON array format."""
    parser = ResponseParser()
    response = """Let me get the time and echo a message.

```json
[
  {"tool": "get_current_time", "arguments": {}},
  {"tool": "echo", "arguments": {"text": "Hello World"}}
]
```

Done!"""
    parsed = parser.parse(response)
    assert len(parsed.tool_calls) == 2
    assert parsed.tool_calls[0].name == "get_current_time"
    assert parsed.tool_calls[1].name == "echo"
    assert parsed.tool_calls[1].arguments == {"text": "Hello World"}


def test_tool_with_arguments():
    """Test parsing a tool call with arguments."""
    parser = ResponseParser()
    response = """I'll add these numbers for you.

```json
{"tool": "add", "arguments": {"a": 15, "b": 27}}
```
"""
    parsed = parser.parse(response)
    assert parsed.has_tool_calls
    assert parsed.tool_calls[0].name == "add"
    assert parsed.tool_calls[0].arguments == {"a": 15, "b": 27}


def test_tool_with_string_arguments():
    """Test parsing a tool call with string arguments."""
    parser = ResponseParser()
    response = """I'll repeat that message.

```json
{"tool": "echo", "arguments": {"text": "Hello, World!"}}
```
"""
    parsed = parser.parse(response)
    assert parsed.has_tool_calls
    assert parsed.tool_calls[0].name == "echo"
    assert parsed.tool_calls[0].arguments == {"text": "Hello, World!"}


def test_no_tool_calls():
    """Test parsing a response with no tool calls."""
    parser = ResponseParser()
    response = "Hello, how can I help you today?"
    parsed = parser.parse(response)
    assert not parsed.has_tool_calls
    assert parsed.text == "Hello, how can I help you today?"


def test_empty_response():
    """Test parsing an empty response."""
    parser = ResponseParser()
    response = ""
    parsed = parser.parse(response)
    assert not parsed.has_tool_calls
    assert parsed.text == ""


def test_malformed_json():
    """Test parsing a response with malformed JSON (should be ignored)."""
    parser = ResponseParser()
    response = """Here's a malformed call:

```json
{"tool": "test", "arguments": {invalid}}
```

And some text after."""
    parsed = parser.parse(response)
    assert not parsed.has_tool_calls
    assert "Here's a malformed call" in parsed.text
    assert "And some text after" in parsed.text


def test_mixed_content_with_tool_call():
    """Test parsing a response with text and tool call mixed."""
    parser = ResponseParser()
    response = """Sure! Let me calculate that for you.

```json
{"tool": "add", "arguments": {"a": 10, "b": 20}}
```

The result will be ready shortly."""
    parsed = parser.parse(response)
    assert parsed.has_tool_calls
    assert "Sure! Let me calculate" in parsed.text
    assert "The result will be ready" in parsed.text
    assert parsed.tool_calls[0].name == "add"


def test_multiple_json_blocks():
    """Test parsing a response with multiple separate JSON blocks."""
    parser = ResponseParser()
    response = """First, let me check the time.

```json
{"tool": "get_current_time", "arguments": {}}
```

Now let me echo a message.

```json
{"tool": "echo", "arguments": {"text": "Testing"}}
```

All done!"""
    parsed = parser.parse(response)
    assert len(parsed.tool_calls) == 2
    assert parsed.tool_calls[0].name == "get_current_time"
    assert parsed.tool_calls[1].name == "echo"
    assert "First, let me check" in parsed.text


def test_tool_call_with_available_tools_validation():
    """Test that tool calls work with available_tools parameter."""
    parser = ResponseParser()
    response = """```json
{"tool": "get_current_time", "arguments": {}}
```"""
    parsed = parser.parse(response, available_tools=["get_current_time", "echo", "add"])
    assert parsed.has_tool_calls
    assert parsed.tool_calls[0].name == "get_current_time"


def test_whitespace_cleaning():
    """Test that extra whitespace is cleaned from the response."""
    parser = ResponseParser()
    response = """Text before.



```json
{"tool": "echo", "arguments": {"text": "test"}}
```


Text after with extra lines.



"""
    parsed = parser.parse(response)
    assert "Text before" in parsed.text
    assert "Text after" in parsed.text
    # Should not have excessive blank lines
    assert "\n\n\n" not in parsed.text


def test_nested_arguments():
    """Test parsing a tool call with nested argument structures."""
    parser = ResponseParser()
    response = """```json
{"tool": "test_tool", "arguments": {"nested": {"key": "value"}, "array": [1, 2, 3]}}
```"""
    parsed = parser.parse(response)
    assert parsed.has_tool_calls
    assert parsed.tool_calls[0].arguments["nested"]["key"] == "value"
    assert parsed.tool_calls[0].arguments["array"] == [1, 2, 3]


def test_has_refusal():
    """Test the refusal detection method."""
    parser = ResponseParser()

    # Test various refusal patterns
    assert parser.has_refusal("I can't do that.")
    assert parser.has_refusal("I'm unable to help with that.")
    assert parser.has_refusal("I'm not allowed to access that.")
    assert parser.has_refusal("I don't know the answer.")
    assert parser.has_refusal("I'm not sure about that.")

    # Test non-refusal responses
    assert not parser.has_refusal("I'll help you with that!")
    assert not parser.has_refusal("Sure, let me check the time.")


def test_extract_code_blocks():
    """Test extracting code blocks from a response."""
    parser = ResponseParser()
    response = """Here's some Python code:

```python
def hello():
    print("Hello, World!")
```

And here's some JavaScript:

```javascript
console.log("Hello!");
```"""
    blocks = parser.extract_code_blocks(response)
    assert len(blocks) == 2
    assert 'def hello():' in blocks[0]
    assert 'console.log' in blocks[1]


def test_extract_code_blocks_with_language_filter():
    """Test extracting code blocks with a language filter."""
    parser = ResponseParser()
    response = """```python
print("Python")
```

```javascript
console.log("JS")
```"""
    python_blocks = parser.extract_code_blocks(response, language="python")
    assert len(python_blocks) == 1
    assert "Python" in python_blocks[0]
    assert "JS" not in python_blocks[0]


def test_extract_json():
    """Test extracting JSON from a response."""
    parser = ResponseParser()
    response = """Some text and then {"key": "value"} and more text."""
    result = parser.extract_json(response)
    assert result == {"key": "value"}


def test_extract_json_not_found():
    """Test extract_json when no JSON is present."""
    parser = ResponseParser()
    response = "This is just plain text with no JSON."
    result = parser.extract_json(response)
    assert result is None


def test_tool_call_to_dict():
    """Test converting ToolCall to dictionary."""
    tool_call = ToolCall(name="test_tool", arguments={"arg1": "value1"})
    result = tool_call.to_dict()
    assert result == {"name": "test_tool", "arguments": {"arg1": "value1"}}


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
