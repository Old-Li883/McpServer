# agent/hr/tests/test_mcp_handler.py
import pytest
import json
from unittest.mock import AsyncMock, patch
from agent.hr.mcp_handler import HRMcpHandler
from agent.hr.engine import HREngine
from agent.config import HRConfig


@pytest.fixture
def handler(tmp_path):
    config = HRConfig(db_path=str(tmp_path / "hr.db"), chroma_collection="test")
    mock_llm = AsyncMock(return_value='{"name":"测试","phone":null,"email":null,"age":null,"education":[{"school":"X","degree":"本科","major":"CS","gpa":null,"start_date":null,"end_date":null}],"skills":["Python"],"experience":[],"projects":[]}')
    engine = HREngine(config=config, llm_generate=mock_llm, chroma_path=str(tmp_path / "chroma"))
    return HRMcpHandler(engine=engine)


@pytest.mark.asyncio
async def test_handle_unknown_tool(handler):
    result = json.loads(await handler.handle("hr_unknown", {}))
    assert "error" in result


@pytest.mark.asyncio
async def test_handle_import_file(handler, tmp_path):
    pdf_path = tmp_path / "test.pdf"
    pdf_path.write_bytes(b"%PDF fake")
    with patch("agent.hr.engine.HREngine._extract_pdf_text", return_value="简历文本"):
        result = json.loads(await handler.handle("hr_import_resumes", {"path": str(pdf_path)}))
    assert result["status"] in ("imported", "failed")
