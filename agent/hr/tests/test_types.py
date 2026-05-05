"""Tests for HR module data models."""

import pytest
from datetime import datetime
from agent.hr.types import Resume, Education, Experience, Project, JobDescription, JdRequirements, MatchResult


def test_education_serialization():
    edu = Education(school="清华大学", degree="本科", major="计算机科学")
    d = edu.to_dict()
    assert d["school"] == "清华大学"
    assert d["degree"] == "本科"
    restored = Education.from_dict(d)
    assert restored.school == "清华大学"


def test_resume_serialization():
    resume = Resume(
        id="abc-123",
        file_name="test.pdf",
        name="张三",
        education=[Education(school="北京大学", degree="硕士", major="软件工程")],
        skills=["Python", "Go"],
        experience=[],
        projects=[],
        raw_text="原始文本",
        parsed_at=datetime(2026, 5, 1),
    )
    d = resume.to_dict()
    assert d["name"] == "张三"
    assert len(d["education"]) == 1
    restored = Resume.from_dict(d)
    assert restored.id == "abc-123"
    assert restored.education[0].school == "北京大学"


def test_jd_requirements_serialization():
    req = JdRequirements(
        min_degree="本科",
        preferred_majors=["计算机", "软件工程"],
        required_skills=["Python"],
        preferred_skills=["Go"],
        experience_years=2,
    )
    d = req.to_dict()
    assert d["min_degree"] == "本科"
    restored = JdRequirements.from_dict(d)
    assert restored.required_skills == ["Python"]


def test_match_result_fields():
    edu = Education(school="X", degree="本科", major="CS")
    resume = Resume(
        id="r1", file_name="r.pdf", name="李四",
        education=[edu], skills=[], experience=[], projects=[],
        raw_text="", parsed_at=datetime(2026, 5, 1),
    )
    result = MatchResult(
        resume=resume,
        overall_score=85.0,
        education_score=90.0,
        skills_score=80.0,
        match_details="优势：学历匹配。不足：缺少Go经验。",
    )
    assert result.overall_score == 85.0


def test_match_result_serialization():
    edu = Education(school="X", degree="本科", major="CS")
    resume = Resume(
        id="r1", file_name="r.pdf", name="李四",
        education=[edu], skills=[], experience=[], projects=[],
        raw_text="", parsed_at=datetime(2026, 5, 1),
    )
    result = MatchResult(
        resume=resume,
        overall_score=85.0,
        education_score=90.0,
        skills_score=80.0,
        match_details="优势：学历匹配。",
    )
    d = result.to_dict()
    assert d["overall_score"] == 85.0
    assert isinstance(d["resume"]["parsed_at"], str)  # datetime serialized to string
    assert d["resume"]["name"] == "李四"
    restored = MatchResult.from_dict(d)
    assert restored.overall_score == 85.0
    assert restored.resume.name == "李四"
