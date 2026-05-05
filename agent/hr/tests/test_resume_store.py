# agent/hr/tests/test_resume_store.py
import pytest
from datetime import datetime
from agent.hr.store.resume_store import ResumeStore
from agent.hr.types import Resume, Education


def make_resume(name: str, file_name: str, degree: str = "本科", skills: list[str] | None = None) -> Resume:
    return Resume(
        file_name=file_name,
        name=name,
        education=[Education(school="测试大学", degree=degree, major="计算机")],
        skills=skills or ["Python"],
        experience=[],
        projects=[],
        raw_text=f"{name}的简历文本",
        parsed_at=datetime(2026, 5, 1),
    )


@pytest.fixture
def store(tmp_path):
    db_path = str(tmp_path / "test_hr.db")
    chroma_path = str(tmp_path / "chroma")
    s = ResumeStore(db_path=db_path, chroma_path=chroma_path, collection_name="test_resumes")
    return s


@pytest.mark.asyncio
async def test_save_and_get(store):
    resume = make_resume("张三", "zhangsan.pdf")
    await store.save(resume)
    fetched = await store.get_by_id(resume.id)
    assert fetched is not None
    assert fetched.name == "张三"
    assert fetched.education[0].school == "测试大学"


@pytest.mark.asyncio
async def test_skip_duplicate_by_filename(store):
    resume = make_resume("张三", "zhangsan.pdf")
    saved = await store.save(resume)
    assert saved is True
    saved_again = await store.save(resume)
    assert saved_again is False


@pytest.mark.asyncio
async def test_force_reparse_overwrites(store):
    resume = make_resume("张三", "zhangsan.pdf")
    await store.save(resume)
    resume2 = make_resume("张三更新", "zhangsan.pdf")
    resume2.id = resume.id
    saved = await store.save(resume2, force=True)
    assert saved is True
    fetched = await store.get_by_id(resume.id)
    assert fetched.name == "张三更新"


@pytest.mark.asyncio
async def test_filter_by_degree(store):
    await store.save(make_resume("本科生", "a.pdf", degree="本科"))
    await store.save(make_resume("硕士生", "b.pdf", degree="硕士"))
    results = await store.filter_by_degree("硕士")
    assert len(results) == 1
    assert results[0].name == "硕士生"


@pytest.mark.asyncio
async def test_semantic_search(store):
    await store.save(make_resume("Python开发", "py.pdf", skills=["Python", "Django"]))
    await store.save(make_resume("Java开发", "java.pdf", skills=["Java", "Spring"]))
    results = await store.semantic_search("Python web开发", top_k=1)
    assert len(results) >= 1
