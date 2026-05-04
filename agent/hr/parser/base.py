"""Base parser abstract class."""

from abc import ABC
from typing import Callable, Awaitable


class BaseParser(ABC):
    """Abstract base for LLM-powered parsers."""

    def __init__(
        self,
        llm_generate: Callable[[str], Awaitable[str]],
        retries: int = 1,
    ):
        self._llm_generate = llm_generate
        self._retries = retries
