"""Tests for ResumeParser."""

import pytest
from unittest.mock import AsyncMock
from agent.hr.parser.resume_parser import ResumeParser
from agent.hr.types import Resume


MOCK_LLM_RESPONSE = """{
  "name": "王五",
  "phone": "13800138000",
  "email": "wangwu@example.com",
  "age": 24,
  "education": [
    {"school": "复旦大学", "degree": "本科", "major": "计算机科学",
     "gpa": "3.8", "start_date": "2020-09", "end_date": "2024-06"}
  ],
  "skills": ["Python", "机器学习", "SQL"],
  "experience": [
    {"company": "字节跳动", "role": "算法实习生", "duration": "6个月",
     "description": "负责推荐系统优化"}
  ],
  "projects": [
    {"name": "毕业设计", "role": "独立开发", "description": "基于BERT的文本分类",
     "tech_stack": ["Python", "PyTorch"]}
  ]
}"""


@pytest.mark.asyncio
async def test_parse_returns_resume():
    parser = ResumeParser(llm_generate=AsyncMock(return_value=MOCK_LLM_RESPONSE))
    resume = await parser.parse_text("简历原始文本", file_name="wangwu.pdf")
    assert isinstance(resume, Resume)
    assert resume.name == "王五"
    assert resume.file_name == "wangwu.pdf"
    assert len(resume.education) == 1
    assert resume.education[0].school == "复旦大学"
    assert "Python" in resume.skills


@pytest.mark.asyncio
async def test_parse_retries_on_invalid_json():
    call_count = 0

    async def flaky_llm(prompt: str) -> str:
        nonlocal call_count
        call_count += 1
        if call_count == 1:
            return "这不是JSON"
        return MOCK_LLM_RESPONSE

    parser = ResumeParser(llm_generate=flaky_llm, retries=1)
    resume = await parser.parse_text("简历文本", file_name="test.pdf")
    assert resume.name == "王五"
    assert call_count == 2


@pytest.mark.asyncio
async def test_parse_raises_after_max_retries():
    parser = ResumeParser(llm_generate=AsyncMock(return_value="not json"), retries=1)
    with pytest.raises(ValueError, match="简历解析失败"):
        await parser.parse_text("简历文本", file_name="bad.pdf")
