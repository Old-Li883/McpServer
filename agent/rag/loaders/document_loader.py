"""Local document loader supporting multiple file formats."""

import os
from pathlib import Path
from typing import Iterator, Optional
import mimetypes

from agent.rag.loaders.base import BaseLoader, LoaderError
from agent.rag.types import Document


class DocumentLoader(BaseLoader):
    """Loader for local documents.

    Supports:
    - Plain text files (.txt, .text)
    - Markdown files (.md, .markdown)
    - HTML files (.html, .htm)
    - PDF files (.pdf) - requires pdfplumber
    - Word documents (.docx) - requires python-docx
    """

    # Supported file extensions
    TEXT_EXTENSIONS = {".txt", ".text"}
    MARKDOWN_EXTENSIONS = {".md", ".markdown"}
    HTML_EXTENSIONS = {".html", ".htm"}
    PDF_EXTENSIONS = {".pdf"}
    DOCX_EXTENSIONS = {".docx"}

    # All supported extensions
    SUPPORTED_EXTENSIONS = (
        TEXT_EXTENSIONS | MARKDOWN_EXTENSIONS | HTML_EXTENSIONS |
        PDF_EXTENSIONS | DOCX_EXTENSIONS
    )

    def __init__(self, encoding: str = "utf-8"):
        """Initialize the document loader.

        Args:
            encoding: Default text encoding (default: utf-8)
        """
        self.encoding = encoding

        # Try to import optional dependencies
        self._pdf_support = self._check_pdf_support()
        self._docx_support = self._check_docx_support()

    def _check_pdf_support(self) -> bool:
        """Check if PDF support is available."""
        try:
            import pdfplumber
            return True
        except ImportError:
            return False

    def _check_docx_support(self) -> bool:
        """Check if DOCX support is available."""
        try:
            import docx
            return True
        except ImportError:
            return False

    def supports(self, source: str) -> bool:
        """Check if source is a supported file."""
        path = Path(source)
        return (
            path.exists() and
            path.is_file() and
            path.suffix.lower() in self.SUPPORTED_EXTENSIONS
        )

    def load(self, source: str, **kwargs) -> Iterator[Document]:
        """Load documents from file(s).

        Args:
            source: File path or directory path
            **kwargs: Additional parameters (recursive for directories)

        Yields:
            Document instances
        """
        path = Path(source)

        if not path.exists():
            raise LoaderError(f"Path does not exist: {source}", source)

        # Handle directory
        if path.is_dir():
            recursive = kwargs.get("recursive", True)
            yield from self._load_directory(path, recursive=recursive)
            return

        # Handle single file
        yield from self._load_file(path)

    def _load_directory(self, directory: Path, recursive: bool = True) -> Iterator[Document]:
        """Load all supported files from a directory."""
        if recursive:
            files = directory.rglob("*")
        else:
            files = directory.iterdir()

        for file in files:
            if file.is_file() and file.suffix.lower() in self.SUPPORTED_EXTENSIONS:
                try:
                    yield from self._load_file(file)
                except Exception as e:
                    # Log error but continue with other files
                    print(f"Warning: Failed to load {file}: {e}")

    def _load_file(self, file_path: Path) -> Iterator[Document]:
        """Load a single file.

        Returns a single-document iterator for consistency.
        """
        suffix = file_path.suffix.lower()

        try:
            if suffix in self.TEXT_EXTENSIONS or suffix in self.MARKDOWN_EXTENSIONS:
                content = self._load_text_file(file_path)
                doc_type = "markdown" if suffix in self.MARKDOWN_EXTENSIONS else "text"
            elif suffix in self.HTML_EXTENSIONS:
                content = self._load_html_file(file_path)
                doc_type = "html"
            elif suffix in self.PDF_EXTENSIONS:
                content = self._load_pdf_file(file_path)
                doc_type = "pdf"
            elif suffix in self.DOCX_EXTENSIONS:
                content = self._load_docx_file(file_path)
                doc_type = "docx"
            else:
                raise LoaderError(f"Unsupported file type: {suffix}", str(file_path))

            # Create document
            doc = Document.create(
                content=content,
                source=str(file_path),
                doc_type=doc_type,
                metadata={
                    "filename": file_path.name,
                    "size": file_path.stat().st_size,
                }
            )

            yield doc

        except Exception as e:
            raise LoaderError(f"Failed to load file: {e}", str(file_path))

    def _load_text_file(self, file_path: Path) -> str:
        """Load a plain text or markdown file."""
        try:
            with open(file_path, "r", encoding=self.encoding) as f:
                return f.read()
        except UnicodeDecodeError:
            # Try with fallback encoding
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                return f.read()

    def _load_html_file(self, file_path: Path) -> str:
        """Load an HTML file and extract text."""
        try:
            from bs4 import BeautifulSoup
            with open(file_path, "r", encoding=self.encoding) as f:
                html = f.read()
            soup = BeautifulSoup(html, "html.parser")
            # Remove script and style elements
            for script in soup(["script", "style"]):
                script.decompose()
            return soup.get_text(separator="\n", strip=True)
        except ImportError:
            raise LoaderError("BeautifulSoup4 is required for HTML support")

    def _load_pdf_file(self, file_path: Path) -> str:
        """Load a PDF file and extract text."""
        if not self._pdf_support:
            raise LoaderError("pdfplumber is required for PDF support")

        try:
            import pdfplumber
            text_parts = []
            with pdfplumber.open(file_path) as pdf:
                for page in pdf.pages:
                    text = page.extract_text()
                    if text:
                        text_parts.append(text)
            return "\n\n".join(text_parts)
        except Exception as e:
            raise LoaderError(f"Failed to extract text from PDF: {e}")

    def _load_docx_file(self, file_path: Path) -> str:
        """Load a Word document and extract text."""
        if not self._docx_support:
            raise LoaderError("python-docx is required for DOCX support")

        try:
            from docx import Document as DocxDocument
            doc = DocxDocument(file_path)
            text_parts = [para.text for para in doc.paragraphs if para.text.strip()]
            return "\n\n".join(text_parts)
        except Exception as e:
            raise LoaderError(f"Failed to extract text from DOCX: {e}")
