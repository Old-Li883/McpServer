"""Resume store: SQLite for structured queries + ChromaDB for semantic search."""

import json
from typing import Optional

import aiosqlite
import chromadb

from agent.hr.store.base import BaseStore
from agent.hr.types import Resume, DEGREE_ORDER

CREATE_TABLE_SQL = """
CREATE TABLE IF NOT EXISTS resumes (
    id TEXT PRIMARY KEY,
    file_name TEXT UNIQUE,
    name TEXT,
    phone TEXT,
    email TEXT,
    age INTEGER,
    education_json TEXT,
    skills_json TEXT,
    experience_json TEXT,
    projects_json TEXT,
    raw_text TEXT,
    parsed_at TEXT
)
"""


class ResumeStore(BaseStore):
    """Dual-storage for resumes: SQLite (structured) + ChromaDB (semantic)."""

    def __init__(self, db_path: str, chroma_path: str, collection_name: str = "hr_resumes"):
        self._db_path = db_path
        self._chroma_client = chromadb.PersistentClient(path=chroma_path)
        self._collection = self._chroma_client.get_or_create_collection(collection_name)

    async def _ensure_table(self, conn: aiosqlite.Connection) -> None:
        await conn.execute(CREATE_TABLE_SQL)
        await conn.commit()

    async def save(self, resume: Resume, force: bool = False) -> bool:
        """Save resume to SQLite and ChromaDB.

        Args:
            resume: The Resume object to save.
            force: If True, overwrite existing entry with same file_name.

        Returns:
            True if saved, False if skipped (duplicate file_name and force=False).
        """
        async with aiosqlite.connect(self._db_path) as conn:
            await self._ensure_table(conn)
            if not force:
                async with conn.execute(
                    "SELECT id FROM resumes WHERE file_name = ?", (resume.file_name,)
                ) as cursor:
                    if await cursor.fetchone():
                        return False
            d = resume.to_dict()
            await conn.execute(
                """INSERT OR REPLACE INTO resumes
                   (id, file_name, name, phone, email, age,
                    education_json, skills_json, experience_json, projects_json,
                    raw_text, parsed_at)
                   VALUES (?,?,?,?,?,?,?,?,?,?,?,?)""",
                (
                    d["id"], d["file_name"], d["name"], d.get("phone"), d.get("email"),
                    d.get("age"), json.dumps(d["education"], ensure_ascii=False),
                    json.dumps(d["skills"], ensure_ascii=False),
                    json.dumps(d["experience"], ensure_ascii=False),
                    json.dumps(d["projects"], ensure_ascii=False),
                    d["raw_text"], d["parsed_at"],
                ),
            )
            await conn.commit()

        self._collection.upsert(
            ids=[resume.id],
            documents=[resume.raw_text],
            metadatas=[{"id": resume.id, "name": resume.name}],
        )
        return True

    async def get_by_id(self, resume_id: str) -> Optional[Resume]:
        """Fetch a resume by ID from SQLite.

        Args:
            resume_id: The UUID of the resume.

        Returns:
            Resume object or None if not found.
        """
        async with aiosqlite.connect(self._db_path) as conn:
            await self._ensure_table(conn)
            conn.row_factory = aiosqlite.Row
            async with conn.execute(
                "SELECT * FROM resumes WHERE id = ?", (resume_id,)
            ) as cursor:
                row = await cursor.fetchone()
        if row is None:
            return None
        return self._row_to_resume(dict(row))

    async def filter_by_degree(self, min_degree: str) -> list[Resume]:
        """Return resumes whose highest degree meets or exceeds min_degree.

        Args:
            min_degree: Minimum degree level (大专/本科/硕士/博士).

        Returns:
            List of matching Resume objects.
        """
        threshold = DEGREE_ORDER.get(min_degree, 0)
        async with aiosqlite.connect(self._db_path) as conn:
            await self._ensure_table(conn)
            conn.row_factory = aiosqlite.Row
            async with conn.execute("SELECT * FROM resumes") as cursor:
                rows = await cursor.fetchall()
        results = []
        for row in rows:
            resume = self._row_to_resume(dict(row))
            max_level = max(
                (DEGREE_ORDER.get(e.degree, 0) for e in resume.education), default=-1
            )
            if max_level >= threshold:
                results.append(resume)
        return results

    async def semantic_search(self, query: str, top_k: int = 5) -> list[Resume]:
        """Search resumes by semantic similarity using ChromaDB.

        Args:
            query: Natural language search query.
            top_k: Maximum number of results to return.

        Returns:
            List of Resume objects sorted by relevance.
        """
        count = self._collection.count()
        if count == 0:
            return []
        results = self._collection.query(
            query_texts=[query], n_results=min(top_k, count)
        )
        ids = results["ids"][0] if results["ids"] else []
        resumes = []
        for rid in ids:
            r = await self.get_by_id(rid)
            if r:
                resumes.append(r)
        return resumes

    def _row_to_resume(self, row: dict) -> Resume:
        row["education"] = json.loads(row["education_json"])
        row["skills"] = json.loads(row["skills_json"])
        row["experience"] = json.loads(row["experience_json"])
        row["projects"] = json.loads(row["projects_json"])
        del row["education_json"], row["skills_json"], row["experience_json"], row["projects_json"]
        return Resume.from_dict(row)
