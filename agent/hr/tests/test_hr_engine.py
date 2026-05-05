# agent/hr/tests/test_hr_engine.py
import pytest
from unittest.mock import AsyncMock, patch
from agent.hr.engine import HREngine
from agent.config import HRConfig


MOCK_RESUME_JSON = """{
  "name": "赵六", "phone": null, "email": null, "age": null,
  "education": [{"school": "上海交大", "degree": "本科", "major": "计算机", "gpa": null, "start_date": null, "end_date": null}],
  "skills": ["Python", "TensorFlow"],
  "experience": [], "projects": []
}"""

MOCK_JD_JSON = """{
  "title": "算法工程师",
  "min_degree": "本科",
  "preferred_majors": ["计算机"],
  "required_skills": ["Python"],
  "preferred_skills": ["TensorFlow"],
  "experience_years": null
}"""


@pytest.fixture
def engine(tmp_path):
    config = HRConfig(
        db_path=str(tmp_path / "hr.db"),
        chroma_collection="test_hr",
        parse_retries=1,
        top_k_default=5,
    )
    mock_llm = AsyncMock(side_effect=[MOCK_RESUME_JSON, MOCK_JD_JSON, "优势：Python匹配。不足：无。建议：录用。"])
    return HREngine(config=config, llm_generate=mock_llm, chroma_path=str(tmp_path / "chroma"))


@pytest.fixture
def engine_for_dup(tmp_path):
    config = HRConfig(
        db_path=str(tmp_path / "hr.db"),
        chroma_collection="test_hr",
        parse_retries=1,
        top_k_default=5,
    )
    mock_llm = AsyncMock(return_value=MOCK_RESUME_JSON)
    return HREngine(config=config, llm_generate=mock_llm, chroma_path=str(tmp_path / "chroma"))


@pytest.mark.asyncio
async def test_import_and_match(engine, tmp_path):
    pdf_path = tmp_path / "test.pdf"
    pdf_path.write_bytes(b"%PDF-1.4 fake")

    with patch("agent.hr.engine.HREngine._extract_pdf_text", return_value="赵六的简历文本"):
        import_result = await engine.import_resume(str(pdf_path))

    assert import_result["status"] == "imported"
    assert import_result["name"] == "赵六"

    results = await engine.match_jd("招聘算法工程师，要求Python技能", top_k=1)
    assert len(results) == 1
    assert results[0].resume.name == "赵六"
    assert results[0].overall_score > 0


@pytest.mark.asyncio
async def test_import_skips_duplicate(engine_for_dup, tmp_path):
    pdf_path = tmp_path / "dup.pdf"
    pdf_path.write_bytes(b"%PDF-1.4 fake")

    with patch("agent.hr.engine.HREngine._extract_pdf_text", return_value="简历文本"):
        r1 = await engine_for_dup.import_resume(str(pdf_path))
        r2 = await engine_for_dup.import_resume(str(pdf_path))

    assert r1["status"] == "imported"
    assert r2["status"] == "skipped"
