"""MatcherEngine: two-stage resume matching against a JD."""

import json
from typing import Callable, Awaitable

from agent.hr.matcher.scorer import Scorer
from agent.hr.types import JobDescription, MatchResult, Resume

MATCH_ANALYSIS_PROMPT = """你是 HR 招聘助手。请根据以下信息，分析候选人与岗位的匹配度。

岗位需求：
{jd_requirements_json}

候选人简历摘要：
{resume_summary_json}

请用 2-3 句中文分析匹配情况，包括：优势、不足、建议。不要打分，只做定性分析。"""


class MatcherEngine:
    """Two-stage matcher: SQLite structural filter + ChromaDB semantic search."""

    def __init__(
        self,
        store,
        scorer: Scorer,
        llm_generate: Callable[[str], Awaitable[str]],
    ):
        self._store = store
        self._scorer = scorer
        self._llm_generate = llm_generate

    async def match(self, jd: JobDescription, top_k: int = 5) -> list[MatchResult]:
        """Match resumes against a JD and return top_k results sorted by score.

        Args:
            jd: The job description to match against.
            top_k: Maximum number of results to return.

        Returns:
            List of MatchResult sorted by overall_score descending.
        """
        req = jd.requirements

        # Stage 1: structural filter by degree
        if req.min_degree and req.min_degree != "不限":
            candidates = await self._store.filter_by_degree(req.min_degree)
        else:
            candidates = await self._store.filter_by_degree("大专")

        # Stage 2: semantic search to enrich candidate pool
        if req.required_skills or req.preferred_skills:
            query = " ".join(req.required_skills + req.preferred_skills)
            semantic_hits = await self._store.semantic_search(query, top_k=top_k * 2)
            seen_ids = {r.id for r in candidates}
            for r in semantic_hits:
                if r.id not in seen_ids:
                    candidates.append(r)
                    seen_ids.add(r.id)

        # Score all candidates
        scored: list[tuple[float, float, float, Resume]] = []
        for resume in candidates:
            edu_s = self._scorer.education_score(resume, req)
            skill_s = self._scorer.skills_score(resume, req)
            overall = self._scorer.overall_score(edu_s, skill_s)
            scored.append((overall, edu_s, skill_s, resume))

        scored.sort(key=lambda x: x[0], reverse=True)
        top = scored[:top_k]

        # Generate LLM analysis for each top candidate
        results = []
        for overall, edu_s, skill_s, resume in top:
            analysis = await self._generate_analysis(jd, resume)
            results.append(MatchResult(
                resume=resume,
                overall_score=round(overall, 1),
                education_score=round(edu_s, 1),
                skills_score=round(skill_s, 1),
                match_details=analysis,
            ))
        return results

    async def _generate_analysis(self, jd: JobDescription, resume: Resume) -> str:
        summary = {
            "name": resume.name,
            "education": [e.to_dict() for e in resume.education],
            "skills": resume.skills,
        }
        prompt = MATCH_ANALYSIS_PROMPT.format(
            jd_requirements_json=json.dumps(jd.requirements.to_dict(), ensure_ascii=False),
            resume_summary_json=json.dumps(summary, ensure_ascii=False),
        )
        return await self._llm_generate(prompt)
