"""Tests for JdStore."""

import pytest
from agent.hr.store.jd_store import JdStore
from agent.hr.types import JobDescription, JdRequirements


@pytest.fixture
def store(tmp_path):
    return JdStore(db_path=str(tmp_path / "test_hr.db"))


@pytest.mark.asyncio
async def test_save_and_get_jd(store):
    jd = JobDescription(
        title="后端工程师",
        raw_text="招聘后端工程师...",
        requirements=JdRequirements(min_degree="本科", required_skills=["Python"]),
    )
    await store.save(jd)
    fetched = await store.get_by_id(jd.id)
    assert fetched is not None
    assert fetched.title == "后端工程师"
    assert fetched.requirements.min_degree == "本科"
