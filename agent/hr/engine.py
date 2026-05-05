"""HREngine: unified entry point for HR resume operations."""

import os
from typing import Any, Callable, Awaitable

import pdfplumber

from agent.config import HRConfig
from agent.hr.matcher.engine import MatcherEngine
from agent.hr.matcher.scorer import Scorer
from agent.hr.parser.jd_parser import JdParser
from agent.hr.parser.resume_parser import ResumeParser
from agent.hr.store.jd_store import JdStore
from agent.hr.store.resume_store import ResumeStore
from agent.hr.types import JobDescription, MatchResult


class HREngine:
    """Orchestrates resume import, parsing, storage, and JD matching."""

    def __init__(
        self,
        config: HRConfig,
        llm_generate: Callable[[str], Awaitable[str]],
        chroma_path: str = "./data/chroma_hr",
    ):
        self._config = config
        self._resume_store = ResumeStore(
            db_path=config.db_path,
            chroma_path=chroma_path,
            collection_name=config.chroma_collection,
        )
        self._jd_store = JdStore(db_path=config.db_path)
        self._resume_parser = ResumeParser(llm_generate=llm_generate, retries=config.parse_retries)
        self._jd_parser = JdParser(llm_generate=llm_generate, retries=config.parse_retries)
        scorer = Scorer(
            education_weight=config.score_weights.get("education", 0.4),
            skills_weight=config.score_weights.get("skills", 0.6),
        )
        self._matcher = MatcherEngine(
            store=self._resume_store, scorer=scorer, llm_generate=llm_generate
        )

    async def import_resume(self, file_path: str, force: bool = False) -> dict[str, Any]:
        """Import a single PDF resume.

        Args:
            file_path: Absolute path to the PDF file.
            force: If True, re-parse and overwrite existing entry.

        Returns:
            Dict with keys: status ('imported'|'skipped'|'failed'), and name/id/file_name on success.
        """
        file_name = os.path.basename(file_path)
        try:
            text = self._extract_pdf_text(file_path)
        except Exception as e:
            return {"status": "failed", "file_name": file_name, "error": str(e)}

        try:
            resume = await self._resume_parser.parse_text(text, file_name=file_name)
        except ValueError as e:
            return {"status": "failed", "file_name": file_name, "error": str(e)}

        saved = await self._resume_store.save(resume, force=force)
        if not saved:
            return {"status": "skipped", "file_name": file_name}
        return {"status": "imported", "id": resume.id, "name": resume.name, "file_name": file_name}

    async def import_directory(self, dir_path: str, force: bool = False) -> dict[str, Any]:
        """Import all PDF files in a directory.

        Args:
            dir_path: Path to directory containing PDF files.
            force: If True, re-parse and overwrite existing entries.

        Returns:
            Summary dict with counts: imported, skipped, failed, and errors list.
        """
        pdf_files = [
            os.path.join(dir_path, f)
            for f in os.listdir(dir_path)
            if f.lower().endswith(".pdf")
        ]
        summary: dict[str, Any] = {"imported": 0, "skipped": 0, "failed": 0, "errors": []}
        for path in pdf_files:
            result = await self.import_resume(path, force=force)
            status = result["status"]
            summary[status] = summary.get(status, 0) + 1
            if status == "failed":
                summary["errors"].append({"file": result["file_name"], "error": result.get("error")})
        return summary

    async def match_jd(self, jd_text: str, top_k: int | None = None) -> list[MatchResult]:
        """Parse JD and return top matching candidates.

        Args:
            jd_text: Raw job description text.
            top_k: Number of candidates to return. Defaults to config.top_k_default.

        Returns:
            List of MatchResult sorted by score descending.
        """
        k = top_k or self._config.top_k_default
        jd = await self._jd_parser.parse(jd_text)
        await self._jd_store.save(jd)
        return await self._matcher.match(jd, top_k=k)

    async def search_candidates(self, query: str, top_k: int | None = None) -> list[MatchResult]:
        """Semantic search for candidates matching a natural language query.

        Args:
            query: Natural language search query.
            top_k: Number of candidates to return. Defaults to config.top_k_default.

        Returns:
            List of MatchResult (scores are 0 for semantic-only search).
        """
        k = top_k or self._config.top_k_default
        resumes = await self._resume_store.semantic_search(query, top_k=k)
        return [
            MatchResult(
                resume=r,
                overall_score=0.0,
                education_score=0.0,
                skills_score=0.0,
                match_details="语义搜索结果",
            )
            for r in resumes
        ]

    async def search_candidates_with_degree_filter(
        self, query: str, min_degree: str, top_k: int | None = None
    ) -> list[MatchResult]:
        """Search candidates combining degree filter and semantic search.

        Args:
            query: Natural language search query.
            min_degree: Minimum degree level (大专/本科/硕士/博士).
            top_k: Number of candidates to return.

        Returns:
            List of MatchResult from combined filter and semantic search.
        """
        k = top_k or self._config.top_k_default
        candidates = await self._resume_store.filter_by_degree(min_degree)
        semantic = await self._resume_store.semantic_search(query, top_k=k * 2)
        seen = {r.id for r in candidates}
        for r in semantic:
            if r.id not in seen:
                candidates.append(r)
                seen.add(r.id)
        return [
            MatchResult(
                resume=r,
                overall_score=0.0,
                education_score=0.0,
                skills_score=0.0,
                match_details="语义搜索结果",
            )
            for r in candidates[:k]
        ]

    def _extract_pdf_text(self, file_path: str) -> str:
        """Extract text from a PDF file using pdfplumber.

        Args:
            file_path: Path to the PDF file.

        Returns:
            Extracted plain text.

        Raises:
            ValueError: If the PDF contains no extractable text (likely a scanned image).
        """
        with pdfplumber.open(file_path) as pdf:
            pages = [page.extract_text() or "" for page in pdf.pages]
        text = "\n".join(pages).strip()
        if not text:
            raise ValueError(f"该 PDF 无法提取文字，可能是扫描件：{file_path}")
        return text
