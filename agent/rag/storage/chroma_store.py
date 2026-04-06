"""ChromaDB vector storage implementation."""

from typing import List, Optional, Dict, Any
from pathlib import Path

import chromadb
from chromadb.config import Settings

from agent.rag.types import Document, SearchResult
from agent.rag.embeddings.ollama_embedder import OllamaEmbedder


class ChromaVectorStore:
    """ChromaDB-based vector storage for RAG.

    Provides persistent storage for document embeddings with
    metadata filtering and similarity search.
    """

    def __init__(
        self,
        path: str = "./data/chroma",
        collection_name: str = "default",
        embedder: Optional[OllamaEmbedder] = None,
    ):
        """Initialize the ChromaDB vector store.

        Args:
            path: Path to ChromaDB storage directory
            collection_name: Name of the collection
            embedder: Optional embedder for generating embeddings
        """
        self.path = Path(path)
        self.collection_name = collection_name
        self.embedder = embedder

        # Create storage directory
        self.path.mkdir(parents=True, exist_ok=True)

        # Initialize ChromaDB client
        self._client = chromadb.PersistentClient(
            path=str(self.path),
            settings=Settings(
                anonymized_telemetry=False,
                allow_reset=True,
            )
        )

        # Get or create collection
        self._collection = self._client.get_or_create_collection(
            name=collection_name,
            metadata={"hnsw:space": "cosine"}
        )

    async def add_documents(
        self,
        documents: List[Document],
    ) -> None:
        """Add documents to the vector store.

        Args:
            documents: List of documents to add
        """
        if not documents:
            return

        # Prepare data for batch insertion
        ids = []
        embeddings = []
        metadatas = []
        contents = []

        for doc in documents:
            ids.append(doc.id)

            # Use pre-computed embedding or generate one
            if doc.embedding:
                embeddings.append(doc.embedding)
            elif self.embedder:
                embedding = await self.embedder.embed(doc.content)
                embeddings.append(embedding)
            else:
                raise ValueError("No embedder provided and document has no embedding")

            # Prepare metadata (ChromaDB doesn't allow nested dicts)
            metadata = {
                "source": doc.metadata.get("source", ""),
                "type": doc.metadata.get("type", "text"),
            }
            # Add flat metadata fields
            for key, value in doc.metadata.items():
                if key not in ["source", "type"] and isinstance(value, (str, int, float, bool)):
                    metadata[key] = value

            metadatas.append(metadata)
            contents.append(doc.content)

        # Add to collection
        self._collection.add(
            ids=ids,
            embeddings=embeddings,
            metadatas=metadatas,
            documents=contents,
        )

    async def search(
        self,
        query: str,
        top_k: int = 5,
        filter: Optional[Dict[str, Any]] = None,
    ) -> List[SearchResult]:
        """Search for similar documents.

        Args:
            query: Query text
            top_k: Number of results to return
            filter: Optional metadata filter

        Returns:
            List of search results with scores
        """
        if not self.embedder:
            raise ValueError("No embedder provided for query")

        # Generate query embedding
        query_embedding = await self.embedder.embed(query)

        # Search
        results = self._collection.query(
            query_embeddings=[query_embedding],
            n_results=top_k,
            where=filter,
        )

        # Convert to SearchResult list
        search_results = []
        if results["ids"] and results["ids"][0]:
            for i, doc_id in enumerate(results["ids"][0]):
                # Reconstruct document
                doc = Document(
                    id=doc_id,
                    content=results["documents"][0][i],
                    metadata=results["metadatas"][0][i] if results["metadatas"] else {},
                )

                # Get distance/score
                # ChromaDB returns distance, convert to similarity score
                distance = results["distances"][0][i]
                score = 1 / (1 + distance)  # Convert to 0-1 range

                search_results.append(SearchResult(
                    document=doc,
                    score=score,
                ))

        return search_results

    def delete(
        self,
        ids: Optional[List[str]] = None,
        filter: Optional[Dict[str, Any]] = None,
    ) -> None:
        """Delete documents from the store.

        Args:
            ids: Optional list of document IDs to delete
            filter: Optional metadata filter for deletion
        """
        if ids:
            self._collection.delete(ids=ids)
        elif filter:
            # ChromaDB doesn't support delete by where directly
            # Need to get IDs first then delete
            results = self._collection.get(where=filter)
            if results["ids"]:
                self._collection.delete(ids=results["ids"])

    def clear(self) -> None:
        """Clear all documents from the collection."""
        self._client.delete_collection(name=self.collection_name)
        self._collection = self._client.create_collection(
            name=self.collection_name,
            metadata={"hnsw:space": "cosine"}
        )

    def count(self) -> int:
        """Get the number of documents in the store."""
        return self._collection.count()

    def get_collection_info(self) -> Dict[str, Any]:
        """Get information about the collection."""
        return {
            "name": self.collection_name,
            "count": self.count(),
            "metadata": self._collection.metadata,
        }

    def close(self) -> None:
        """Close the vector store."""
        # ChromaDB persistent client doesn't need explicit close
        pass
