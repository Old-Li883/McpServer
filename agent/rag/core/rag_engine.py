"""RAG engine implementation with chunking support."""

import asyncio
from typing import List, Optional, Dict, Any
from pathlib import Path

from agent.rag.types import Document, QueryResult, SearchResult, RAGConfig
from agent.rag.embeddings.ollama_embedder import OllamaEmbedder
from agent.rag.storage.chroma_store import ChromaVectorStore
from agent.rag.loaders.factory import get_global_registry
from agent.rag.processors.chunker import (
    BaseChunker,
    Chunk,
    ChunkerFactory,
)
from agent.rag.processors.preprocessor import TextPreprocessor
from agent.rag.processors.chinese_optimizer import (
    ChineseTextProcessor,
    ChineseSentenceSplitter,
)


class RAGEngine:
    """Main RAG engine that coordinates all components.

    This engine provides:
    - Document loading and preprocessing
    - Intelligent chunking
    - Vector storage and retrieval
    - Query processing with LLM integration
    """

    def __init__(
        self,
        config: Optional[RAGConfig] = None,
    ):
        """Initialize the RAG engine.

        Args:
            config: RAG configuration (uses defaults if not provided)
        """
        self.config = config or RAGConfig()

        # Initialize components
        self._embedder: Optional[OllamaEmbedder] = None
        self._vector_store: Optional[ChromaVectorStore] = None
        self._loader_registry = get_global_registry()

        # Preprocessing and chunking
        self._preprocessor = TextPreprocessor()
        self._chinese_processor: Optional[ChineseTextProcessor] = None
        self._chunker: Optional[BaseChunker] = None

        # Runtime state
        self._initialized = False

        # Statistics
        self._stats = {
            "documents_loaded": 0,
            "chunks_created": 0,
            "queries_processed": 0,
        }

    async def initialize(self) -> None:
        """Initialize the RAG engine components.

        This must be called before using the engine.
        """
        if self._initialized:
            return

        # Initialize embedder
        self._embedder = OllamaEmbedder(
            model=self.config.embedder_model,
            base_url=self.config.embedder_base_url,
            cache_path=self.config.cache_path,
            enable_cache=self.config.enable_cache,
        )

        # Initialize vector store
        self._vector_store = ChromaVectorStore(
            path=self.config.vector_db_path,
            collection_name=self.config.collection_name,
            embedder=self._embedder,
        )

        # Initialize Chinese processor if enabled
        if self.config.enable_chinese_optimization:
            self._chinese_processor = ChineseTextProcessor(
                enable_segmentation=True,
                preserve_phrases=True,
                mixed_language_handling=True,
            )

        # Initialize chunker
        self._chunker = ChunkerFactory.create(
            strategy="semantic",  # Use semantic chunking by default
            chunk_size=self.config.chunk_size,
            chunk_overlap=self.config.chunk_overlap,
        )

        self._initialized = True

    async def add_documents(
        self,
        source: str,
        chunk_strategy: Optional[str] = None,
        **kwargs
    ) -> int:
        """Load and add documents from a source.

        Args:
            source: Source path (file, directory, URL, etc.)
            chunk_strategy: Override chunking strategy (auto, fixed, semantic, markdown, code)
            **kwargs: Additional loader parameters

        Returns:
            Number of chunks added

        Raises:
            RuntimeError: If engine is not initialized
            LoaderError: If document loading fails
        """
        if not self._initialized:
            await self.initialize()

        # Load documents
        documents = []
        async for doc in self._loader_registry.load_async(source, **kwargs):
            documents.append(doc)

        if not documents:
            return 0

        # Preprocess documents
        processed_docs = []
        for doc in documents:
            # Apply preprocessing
            content = self._preprocessor.preprocess(doc.content)
            if not content:
                continue

            # Apply Chinese optimization if enabled
            if self._chinese_processor and self._chinese_processor.is_chinese(content):
                content = self._chinese_processor.optimize_for_search(content)

            # Create processed document
            processed_doc = Document(
                id=doc.id,
                content=content,
                metadata=doc.metadata,
            )
            processed_docs.append(processed_doc)

        # Chunk documents
        all_chunks = []
        for doc in processed_docs:
            # Select chunker
            if chunk_strategy:
                chunker = ChunkerFactory.create(
                    strategy=chunk_strategy,
                    chunk_size=self.config.chunk_size,
                    chunk_overlap=self.config.chunk_overlap,
                )
            else:
                # Auto-detect chunker
                chunker = ChunkerFactory.auto_detect(
                    doc,
                    chunk_size=self.config.chunk_size,
                    chunk_overlap=self.config.chunk_overlap,
                )

            # Chunk the document
            chunks = chunker.chunk(doc)

            # Convert chunks to documents
            for chunk in chunks:
                chunk_doc = chunk.to_document()
                all_chunks.append(chunk_doc)

        # Add chunks to vector store
        await self._vector_store.add_documents(all_chunks)

        # Update stats
        self._stats["documents_loaded"] += len(documents)
        self._stats["chunks_created"] += len(all_chunks)

        return len(all_chunks)

    async def query(
        self,
        query_text: str,
        top_k: Optional[int] = None,
        **kwargs
    ) -> QueryResult:
        """Query the knowledge base.

        Args:
            query_text: The query text
            top_k: Number of results to retrieve (uses config default if None)
            **kwargs: Additional query parameters

        Returns:
            QueryResult with answer and sources

        Raises:
            RuntimeError: If engine is not initialized
        """
        if not self._initialized:
            await self.initialize()

        top_k = top_k or self.config.top_k

        # Optimize query for Chinese if needed
        optimized_query = query_text
        if self._chinese_processor and self._chinese_processor.is_chinese(query_text):
            optimized_query = self._chinese_processor.optimize_for_search(query_text)

        # Search for relevant documents
        sources = await self._vector_store.search(
            query=optimized_query,
            top_k=top_k,
            **kwargs
        )

        # Filter by score threshold
        filtered_sources = [
            s for s in sources
            if s.score >= self.config.score_threshold
        ]

        # Build context from sources
        context = self._build_context(filtered_sources)

        # Generate answer
        answer = self._generate_answer(query_text, context, filtered_sources)

        # Update stats
        self._stats["queries_processed"] += 1

        return QueryResult(
            answer=answer,
            sources=filtered_sources,
            query=query_text,
            retrieved_docs=[s.document for s in filtered_sources],
            metadata={
                "chunks_retrieved": len(sources),
                "chunks_used": len(filtered_sources),
            }
        )

    def _build_context(self, sources: List[SearchResult]) -> str:
        """Build context string from search results.

        Args:
            sources: Search results

        Returns:
            Formatted context string
        """
        if not sources:
            return "No relevant information found in the knowledge base. If you don't have relevant context, say you don't know."

        context_parts = []
        context_parts.append("# Knowledge Base Search Results\n")
        context_parts.append("The following are relevant documents retrieved from the knowledge base. Please answer **ONLY** based on this information:\n")

        for i, source in enumerate(sources, 1):
            # Get source metadata
            source_name = source.document.metadata.get("source", "Unknown")
            chunk_info = ""

            if "chunk_id" in source.document.metadata:
                chunk_info = f" [Chunk: {source.document.metadata['chunk_id']}]"

            context_parts.append(
                f"## [Source {i}] (relevance: {source.score:.1%}){chunk_info}\n"
                f"{source.document.content}\n"
            )

        context_parts.append("\n---\n")
        context_parts.append("**IMPORTANT REMINDERS**:\n")
        context_parts.append("- You MUST cite specific sources [Source X] when answering\n")
        context_parts.append("- If there's no answer in the above content, clearly say \"Based on available materials, I cannot answer\"\n")
        context_parts.append("- Do NOT add information from outside the knowledge base\n")

        return "\n".join(context_parts)

    def _generate_answer(
        self,
        query: str,
        context: str,
        sources: List[SearchResult]
    ) -> str:
        """Generate answer from query and context.

        TODO: This should use the LLM client.
        For now, returns a simple formatted response.
        """
        if not sources:
            return f"## Search Results\n\nNo relevant information found in the knowledge base for:\n\nQuestion: {query}\n\nSuggestions:\n- Check if the question is phrased accurately\n- Try using different keywords\n- Or contact admin to add relevant documents"

        # Check if max score is too low
        max_score = max(s.score for s in sources)
        if max_score < 0.6:
            return f"## Search Results\n\nFound some possibly relevant documents (low relevance, max {max_score:.1%}):\n\n{context}\n\n⚠️ Note: The above documents have low relevance, the answer may not be accurate."

        answer_parts = [
            f"## Search Results\n\nFound {len(sources)} relevant document(s) (relevance: {max_score:.1%} - {min(s.score for s in sources):.1%}):\n",
            context,
        ]

        return "\n".join(answer_parts)

    async def delete_collection(self) -> None:
        """Delete all documents from the current collection."""
        if not self._initialized:
            await self.initialize()

        self._vector_store.clear()
        self._stats = {
            "documents_loaded": 0,
            "chunks_created": 0,
            "queries_processed": 0,
        }

    def get_stats(self) -> Dict[str, Any]:
        """Get statistics about the RAG engine.

        Returns:
            Dictionary with engine statistics
        """
        base_stats = {
            "initialized": self._initialized,
            "config": self.config.to_dict(),
            "usage_stats": self._stats.copy(),
        }

        if not self._initialized:
            return base_stats

        base_stats["collection_info"] = self._vector_store.get_collection_info()
        base_stats["available_loaders"] = [
            loader.__class__.__name__ for loader in self._loader_registry.loaders
        ]
        base_stats["chinese_optimization_enabled"] = self._chinese_processor is not None

        return base_stats

    async def close(self) -> None:
        """Close the RAG engine and release resources."""
        if self._embedder:
            await self._embedder.close()

        if self._vector_store:
            self._vector_store.close()

        self._initialized = False

    async def __aenter__(self):
        """Async context manager entry."""
        await self.initialize()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit."""
        await self.close()
