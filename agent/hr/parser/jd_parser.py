"""JD parser: JD text -> structured JobDescription via LLM."""

import json
import re
from typing import Callable, Awaitable

from agent.hr.parser.base import BaseParser
from agent.hr.types import JobDescription, JdRequirements

JD_PARSE_PROMPT = """你是招聘需求分析助手。请从以下岗位描述中提取关键需求，按 JSON 格式输出：

{{
  "title": "岗位名称",
  "min_degree": "最低学历要求（本科/硕士/博士/不限）",
  "preferred_majors": ["倾向专业"],
  "required_skills": ["必备技能"],
  "preferred_skills": ["加分技能"],
  "experience_years": 经验年限要求（数字或null）
}}

岗位描述：
---
{jd_text}
---

请只输出 JSON，不要输出其他内容。"""


def _extract_json(text: str) -> str:
    """Extract JSON from text, handling markdown code blocks."""
    match = re.search(r"```(?:json)?\s*([\s\S]+?)\s*```", text)
    if match:
        return match.group(1)
    return text.strip()


class JdParser(BaseParser):
    """Parses JD text into a structured JobDescription using LLM."""

    async def parse(self, jd_text: str) -> JobDescription:
        """Parse JD text into a JobDescription object.

        Args:
            jd_text: Raw job description text.

        Returns:
            Parsed JobDescription object.

        Raises:
            ValueError: If LLM output cannot be parsed after retries.
        """
        prompt = JD_PARSE_PROMPT.format(jd_text=jd_text)
        last_error: Exception | None = None

        for attempt in range(self._retries + 1):
            raw = await self._llm_generate(prompt)
            try:
                data = json.loads(_extract_json(raw))
                requirements = JdRequirements(
                    min_degree=data.get("min_degree"),
                    preferred_majors=data.get("preferred_majors", []),
                    required_skills=data.get("required_skills", []),
                    preferred_skills=data.get("preferred_skills", []),
                    experience_years=data.get("experience_years"),
                )
                return JobDescription(
                    title=data.get("title", "未知岗位"),
                    raw_text=jd_text,
                    requirements=requirements,
                )
            except (json.JSONDecodeError, KeyError, TypeError) as e:
                last_error = e

        raise ValueError(f"JD解析失败：{last_error}")
