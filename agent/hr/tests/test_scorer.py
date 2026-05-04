# agent/hr/tests/test_scorer.py
import pytest
from agent.hr.matcher.scorer import Scorer
from agent.hr.types import Resume, Education, JdRequirements
from datetime import datetime


def make_resume(degree: str, skills: list[str]) -> Resume:
    return Resume(
        file_name="test.pdf", name="测试",
        education=[Education(school="X", degree=degree, major="CS")],
        skills=skills, experience=[], projects=[],
        raw_text="", parsed_at=datetime(2026, 5, 1),
    )


def make_jd_req(min_degree: str, required_skills: list[str], preferred_skills: list[str] | None = None) -> JdRequirements:
    return JdRequirements(
        min_degree=min_degree,
        required_skills=required_skills,
        preferred_skills=preferred_skills or [],
    )


def test_education_score_exact_match():
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    resume = make_resume("本科", ["Python"])
    req = make_jd_req("本科", ["Python"])
    score = scorer.education_score(resume, req)
    assert score == 100.0


def test_education_score_exceeds_requirement():
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    resume = make_resume("硕士", ["Python"])
    req = make_jd_req("本科", ["Python"])
    score = scorer.education_score(resume, req)
    assert score == 100.0


def test_education_score_below_requirement():
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    resume = make_resume("大专", ["Python"])
    req = make_jd_req("本科", ["Python"])
    score = scorer.education_score(resume, req)
    assert score < 100.0


def test_skills_score_full_match():
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    resume = make_resume("本科", ["Python", "MySQL", "Go"])
    req = make_jd_req("本科", ["Python", "MySQL"], preferred_skills=["Go"])
    score = scorer.skills_score(resume, req)
    assert score == 100.0


def test_skills_score_partial_match():
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    resume = make_resume("本科", ["Python"])
    req = make_jd_req("本科", ["Python", "MySQL"])
    score = scorer.skills_score(resume, req)
    assert 0 < score < 100.0


def test_overall_score_weighted():
    scorer = Scorer(education_weight=0.4, skills_weight=0.6)
    resume = make_resume("本科", ["Python", "MySQL"])
    req = make_jd_req("本科", ["Python", "MySQL"])
    edu_s = scorer.education_score(resume, req)
    skill_s = scorer.skills_score(resume, req)
    overall = scorer.overall_score(edu_s, skill_s)
    assert overall == pytest.approx(edu_s * 0.4 + skill_s * 0.6)
