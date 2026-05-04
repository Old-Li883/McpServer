"""MCP tool handler for HR tools: routes C++ tool calls to HREngine."""

import json
import os
from typing import Any

from agent.hr.engine import HREngine
from agent.hr.types import MatchResult


class HRMcpHandler:
    """Handles HR tool calls forwarded from the C++ MCP server."""

    def __init__(self, engine: HREngine):
        self._engine = engine

    async def handle(self, tool_name: str, args: dict[str, Any]) -> str:
        """Dispatch a tool call to the appropriate HREngine method.

        Args:
            tool_name: One of hr_import_resumes, hr_parse_resume,
                       hr_search_candidates, hr_match_jd.
            args: Tool arguments dict.

        Returns:
            JSON string result to return to the caller.
        """
        if tool_name == "hr_import_resumes":
            return await self._handle_import(args)
        elif tool_name == "hr_parse_resume":
            return await self._handle_parse(args)
        elif tool_name == "hr_search_candidates":
            return await self._handle_search(args)
        elif tool_name == "hr_match_jd":
            return await self._handle_match_jd(args)
        else:
            return json.dumps({"error": f"未知 HR 工具：{tool_name}"}, ensure_ascii=False)

    async def _handle_import(self, args: dict[str, Any]) -> str:
        path = args["path"]
        force = args.get("force_reparse", False)
        if os.path.isdir(path):
            result = await self._engine.import_directory(path, force=force)
        else:
            result = await self._engine.import_resume(path, force=force)
        return json.dumps(result, ensure_ascii=False)

    async def _handle_parse(self, args: dict[str, Any]) -> str:
        result = await self._engine.import_resume(args["file_path"], force=False)
        return json.dumps(result, ensure_ascii=False)

    async def _handle_search(self, args: dict[str, Any]) -> str:
        query = args["query"]
        top_k = args.get("top_k", 5)
        filters = args.get("filters") or {}
        min_degree = filters.get("min_degree")

        if min_degree:
            candidates = await self._engine._resume_store.filter_by_degree(min_degree)
            semantic = await self._engine._resume_store.semantic_search(query, top_k=top_k * 2)
            seen = {r.id for r in candidates}
            for r in semantic:
                if r.id not in seen:
                    candidates.append(r)
                    seen.add(r.id)
            results = [
                MatchResult(
                    resume=r,
                    overall_score=0.0,
                    education_score=0.0,
                    skills_score=0.0,
                    match_details="语义搜索结果",
                )
                for r in candidates[:top_k]
            ]
        else:
            results = await self._engine.search_candidates(query, top_k=top_k)
        return json.dumps(self._format_results(results), ensure_ascii=False)

    async def _handle_match_jd(self, args: dict[str, Any]) -> str:
        jd_text = args["jd_text"]
        top_k = args.get("top_k", 5)
        results = await self._engine.match_jd(jd_text, top_k=top_k)
        return json.dumps(self._format_results(results), ensure_ascii=False)

    def _format_results(self, results: list[MatchResult]) -> list[dict[str, Any]]:
        """Format MatchResult list for JSON output.

        Args:
            results: List of MatchResult objects.

        Returns:
            List of dicts suitable for JSON serialization.
        """
        return [
            {
                "name": r.resume.name,
                "file_name": r.resume.file_name,
                "overall_score": r.overall_score,
                "education_score": r.education_score,
                "skills_score": r.skills_score,
                "match_details": r.match_details,
                "education": [e.to_dict() for e in r.resume.education],
                "skills": r.resume.skills,
            }
            for r in results
        ]
