"""JD store: SQLite persistence for job descriptions."""

import json
from typing import Optional

import aiosqlite

from agent.hr.store.base import BaseStore
from agent.hr.types import JobDescription, JdRequirements

CREATE_TABLE_SQL = """
CREATE TABLE IF NOT EXISTS jds (
    id TEXT PRIMARY KEY,
    title TEXT,
    raw_text TEXT,
    requirements_json TEXT,
    created_at TEXT
)
"""


class JdStore(BaseStore):
    """SQLite storage for job descriptions."""

    def __init__(self, db_path: str):
        self._db_path = db_path

    async def _ensure_table(self, conn: aiosqlite.Connection) -> None:
        await conn.execute(CREATE_TABLE_SQL)
        await conn.commit()

    async def save(self, jd: JobDescription) -> None:
        """Persist a JobDescription to SQLite.

        Args:
            jd: The JobDescription to save.
        """
        async with aiosqlite.connect(self._db_path) as conn:
            await self._ensure_table(conn)
            d = jd.to_dict()
            await conn.execute(
                "INSERT OR REPLACE INTO jds (id, title, raw_text, requirements_json, created_at) VALUES (?,?,?,?,?)",
                (d["id"], d["title"], d["raw_text"],
                 json.dumps(d["requirements"], ensure_ascii=False), d["created_at"]),
            )
            await conn.commit()

    async def get_by_id(self, jd_id: str) -> Optional[JobDescription]:
        """Fetch a JobDescription by ID.

        Args:
            jd_id: The UUID of the job description.

        Returns:
            JobDescription object or None if not found.
        """
        async with aiosqlite.connect(self._db_path) as conn:
            await self._ensure_table(conn)
            conn.row_factory = aiosqlite.Row
            async with conn.execute("SELECT * FROM jds WHERE id = ?", (jd_id,)) as cursor:
                row = await cursor.fetchone()
        if row is None:
            return None
        d = dict(row)
        d["requirements"] = JdRequirements.from_dict(json.loads(d["requirements_json"]))
        del d["requirements_json"]
        return JobDescription.from_dict(d)
