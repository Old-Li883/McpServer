# agent/hr/tests/test_matcher_engine.py
import pytest
from unittest.mock import AsyncMock, MagicMock
from datetime import datetime
from agent.hr.matcher.engine import MatcherEngine
from agent.hr.matcher.scorer import Scorer
from agent.hr.types import Resume, Education, JobDescription, JdRequirements, MatchResult


def make_resume(name: str, degree: str, skills: list[str]) -> Resume:
    return Resume(
        file_name=f"{name}.pdf", name=name,
        education=[Education(school="X", degree=degree, major="CS")],
        skills=skills, experience=[], projects=[],
        raw_text=f"{name} {' '.join(skills)}", parsed_at=datetime(2026, 5, 1),
    )


def make_jd(min_degree: str, required_skills: list[str]) -> JobDescription:
    return JobDescription(
        title="测试岗位", raw_text="测试JD",
        requirements=JdRequirements(min_degree=min_degree, required_skills=required_skills),
    )


@pytest.mark.asyncio
async def test_match_returns_sorted_results():
    r1 = make_resume("高分候选人", "本科", ["Python", "MySQL"])
    r2 = make_resume("低分候选人", "大专", ["Java"])

    mock_store = MagicMock()
    mock_store.filter_by_degree = AsyncMock(return_value=[r1, r2])
    mock_store.semantic_search = AsyncMock(return_value=[r1, r2])

    mock_llm = AsyncMock(return_value="优势：技能匹配。不足：无。建议：录用。")
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    engine = MatcherEngine(store=mock_store, scorer=scorer, llm_generate=mock_llm)

    jd = make_jd("本科", ["Python", "MySQL"])
    results = await engine.match(jd, top_k=2)

    assert len(results) <= 2
    assert all(isinstance(r, MatchResult) for r in results)
    assert results[0].overall_score >= results[-1].overall_score


@pytest.mark.asyncio
async def test_match_generates_llm_analysis():
    r1 = make_resume("候选人A", "本科", ["Python"])
    mock_store = MagicMock()
    mock_store.filter_by_degree = AsyncMock(return_value=[r1])
    mock_store.semantic_search = AsyncMock(return_value=[r1])

    analysis_text = "优势：Python技能匹配。不足：缺少MySQL。建议：可考虑。"
    mock_llm = AsyncMock(return_value=analysis_text)
    scorer = Scorer()
    engine = MatcherEngine(store=mock_store, scorer=scorer, llm_generate=mock_llm)

    jd = make_jd("本科", ["Python"])
    results = await engine.match(jd, top_k=1)
    assert results[0].match_details == analysis_text
