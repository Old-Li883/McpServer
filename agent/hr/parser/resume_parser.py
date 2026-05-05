"""Resume parser: PDF text -> structured Resume via LLM."""

import json
import re
from typing import Callable, Awaitable

from agent.hr.parser.base import BaseParser
from agent.hr.types import Resume

RESUME_PARSE_PROMPT = """你是一个专业的简历解析助手。请从以下简历文本中提取结构化信息，严格按 JSON 格式输出。

输出格式：
{{
  "name": "姓名",
  "phone": "电话（如有，否则null）",
  "email": "邮箱（如有，否则null）",
  "age": 年龄数字或null,
  "education": [
    {{"school": "学校全称", "degree": "本科/硕士/博士/大专/其他", "major": "专业",
      "gpa": "GPA（如有，否则null）", "start_date": "入学时间", "end_date": "毕业时间"}}
  ],
  "skills": ["技能1", "技能2"],
  "experience": [
    {{"company": "公司/组织名", "role": "岗位", "duration": "时长", "description": "工作内容"}}
  ],
  "projects": [
    {{"name": "项目名", "role": "角色（如有，否则null）", "description": "项目描述",
      "tech_stack": ["技术1", "技术2"]}}
  ]
}}

简历文本：
---
{resume_text}
---

请只输出 JSON，不要输出其他内容。"""


def _extract_json(text: str) -> str:
    """Extract JSON block from LLM output that may contain markdown fences."""
    match = re.search(r"```(?:json)?\s*([\s\S]+?)\s*```", text)
    if match:
        return match.group(1)
    return text.strip()


class ResumeParser(BaseParser):
    """Parses resume text into a structured Resume object using LLM."""

    def __init__(
        self,
        llm_generate: Callable[[str], Awaitable[str]],
        retries: int = 1,
    ):
        super().__init__(llm_generate, retries)

    async def parse_text(self, text: str, file_name: str) -> Resume:
        """Parse resume plain text into a Resume object.

        Args:
            text: Raw resume text extracted from PDF.
            file_name: Original PDF file name (used as identifier).

        Returns:
            Parsed Resume object.

        Raises:
            ValueError: If LLM output cannot be parsed after retries.
        """
        prompt = RESUME_PARSE_PROMPT.format(resume_text=text)
        last_error: Exception | None = None

        for attempt in range(self._retries + 1):
            raw = await self._llm_generate(prompt)
            try:
                data = json.loads(_extract_json(raw))
                data["file_name"] = file_name
                data["raw_text"] = text
                return Resume.from_dict(data)
            except (json.JSONDecodeError, KeyError, TypeError) as e:
                last_error = e

        raise ValueError(f"简历解析失败（{file_name}）：{last_error}")
