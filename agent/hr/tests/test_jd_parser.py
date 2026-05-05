"""Tests for JdParser."""

import pytest
from unittest.mock import AsyncMock

from agent.hr.parser.jd_parser import JdParser
from agent.hr.types import JobDescription

MOCK_JD_RESPONSE = """{
  "title": "后端工程师",
  "min_degree": "本科",
  "preferred_majors": ["计算机科学", "软件工程"],
  "required_skills": ["Python", "MySQL"],
  "preferred_skills": ["Go", "Redis"],
  "experience_years": 2
}"""


@pytest.mark.asyncio
async def test_parse_jd_returns_job_description():
    parser = JdParser(llm_generate=AsyncMock(return_value=MOCK_JD_RESPONSE))
    jd = await parser.parse("招聘后端工程师，要求本科及以上...")
    assert isinstance(jd, JobDescription)
    assert jd.title == "后端工程师"
    assert jd.requirements.min_degree == "本科"
    assert "Python" in jd.requirements.required_skills
    assert jd.requirements.experience_years == 2


@pytest.mark.asyncio
async def test_parse_jd_raises_on_persistent_failure():
    parser = JdParser(llm_generate=AsyncMock(return_value="invalid"), retries=1)
    with pytest.raises(ValueError, match="JD解析失败"):
        await parser.parse("some jd text")
