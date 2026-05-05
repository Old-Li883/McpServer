# HR 简历筛选 Agent 设计文档

**日期**: 2026-05-04
**状态**: 已确认

## 概述

基于现有 MCP Server 项目构建一个本地 HR 简历筛选智能 Agent。支持两种交互模式：

1. **对话式问答** — 自然语言提问，如"帮我找一个会 Python 的计算机专业本科生"
2. **JD 匹配模式** — 输入岗位 JD，自动解析需求，搜索评分，输出 Top N 候选人

技术选型：C++ MCP Server 注册 HR 工具 + Python Agent 实现业务逻辑，LLM 使用 qwen2.5:7b（本地 Ollama），数据存储使用 SQLite + ChromaDB。

## 需求

- 简历格式：PDF
- 简历规模：<100 份（小规模，简洁优先）
- 匹配维度：教育背景 + 技能匹配（核心），工作经验和基本条件为辅
- 输出形式：终端文本输出
- 部署：完全本地运行

## 整体架构

```
用户 (CLI)
  │
  ▼
Python Agent (agent/)
  ├─ CLI 层 ── 接收用户输入，区分「自由对话」和「JD 匹配」意图
  ├─ HR Engine (agent/hr/) ── 核心业务逻辑
  │    ├─ ResumeParser ── 调用 LLM 从 PDF 提取结构化信息
  │    ├─ JdParser ── 调用 LLM 解析 JD 需求
  │    ├─ Matcher ── 结构化匹配 + 语义匹配打分
  │    └─ ResumeStore / JdStore ── SQLite 持久化 + ChromaDB 向量索引
  ├─ MCP Client ── 通过 JSON-RPC 调用 C++ 端工具
  └─ 现有 RAG / Memory 系统 ── 复用
        │
        ▼
C++ MCP Server (src/)
  ├─ hr_import_resumes ── 批量导入 PDF，触发解析和索引
  ├─ hr_parse_resume   ── 解析单份简历为 JSON
  ├─ hr_search_candidates ── 按条件搜索候选人
  └─ hr_match_jd       ── JD 匹配，返回 Top N 候选人
```

### 数据流：导入简历

1. 用户说"导入简历" → CLI 识别意图
2. Agent 调用 MCP `hr_import_resumes`，传入 PDF 目录路径
3. C++ 工具将请求转发给 Python 端（通过回调）
4. Python `ResumeParser` 用 pdfplumber 提取文本
5. LLM (qwen2.5:7b) 做 NER 提取结构化字段
6. 结构化数据存 SQLite，简历文本存 ChromaDB
7. 返回导入结果

### 数据流：JD 匹配

1. 用户输入 JD 文本
2. Agent 调用 MCP `hr_match_jd`，传入 JD 内容
3. Python `JdParser` 提取 JD 关键需求（学历、技能等）
4. `Matcher` 两阶段匹配：
   - 第一阶段：SQLite 结构化筛选（学历、专业硬条件）
   - 第二阶段：ChromaDB 语义搜索（技能、经验相关度）
5. 综合打分，返回 Top N 候选人

## 数据模型

### Resume

```python
class Resume(BaseModel):
    id: str                          # UUID
    file_name: str                   # 原始文件名
    name: str                        # 姓名
    phone: str | None                # 电话
    email: str | None                # 邮箱
    age: int | None                  # 年龄
    education: list[Education]       # 教育经历
    skills: list[str]                # 技能列表
    experience: list[Experience]     # 实习/工作经历
    projects: list[Project]          # 项目经历
    raw_text: str                    # 原始文本（存 ChromaDB）
    parsed_at: datetime              # 解析时间
```

### Education

```python
class Education(BaseModel):
    school: str                      # 学校名
    degree: str                      # 学位：本科/硕士/博士
    major: str                       # 专业
    gpa: str | None                  # GPA
    start_date: str | None           # 入学时间
    end_date: str | None             # 毕业时间
```

### Experience

```python
class Experience(BaseModel):
    company: str                     # 公司/组织
    role: str                        # 岗位
    duration: str                    # 时长
    description: str                 # 工作内容描述
```

### Project

```python
class Project(BaseModel):
    name: str                        # 项目名
    role: str | None                 # 角色
    description: str                 # 项目描述
    tech_stack: list[str]            # 使用的技术
```

### JobDescription

```python
class JobDescription(BaseModel):
    id: str                          # UUID
    title: str                       # 岗位名称
    raw_text: str                    # JD 原文
    requirements: JdRequirements     # 提取的需求
    created_at: datetime
```

### JdRequirements

```python
class JdRequirements(BaseModel):
    min_degree: str | None           # 最低学历要求
    preferred_majors: list[str]      # 倾向专业
    required_skills: list[str]       # 必备技能
    preferred_skills: list[str]      # 加分技能
    experience_years: int | None     # 经验年限要求
```

### MatchResult

```python
class MatchResult(BaseModel):
    resume: Resume                   # 候选人简历
    overall_score: float             # 综合分数 0-100
    education_score: float           # 教育匹配分
    skills_score: float              # 技能匹配分
    match_details: str               # LLM 生成的匹配分析文本
```

## C++ MCP 工具定义

### hr_import_resumes

批量导入 PDF 简历。支持单个文件或目录路径。

```json
{
  "name": "hr_import_resumes",
  "description": "批量导入 PDF 简历。支持传入单个文件路径或目录路径，自动解析并存储。",
  "inputSchema": {
    "type": "object",
    "properties": {
      "path": { "type": "string", "description": "PDF 文件路径或包含 PDF 的目录路径" },
      "force_reparse": { "type": "boolean", "description": "是否强制重新解析已存在的简历", "default": false }
    },
    "required": ["path"]
  }
}
```

返回：导入摘要（成功数、失败数、跳过数）

### hr_parse_resume

解析单份 PDF 简历，返回结构化信息。

```json
{
  "name": "hr_parse_resume",
  "description": "解析单份 PDF 简历，返回结构化信息（姓名、学历、技能等）。如果简历已导入，直接从数据库返回。",
  "inputSchema": {
    "type": "object",
    "properties": {
      "file_path": { "type": "string", "description": "PDF 文件路径" }
    },
    "required": ["file_path"]
  }
}
```

返回：完整 Resume JSON

### hr_search_candidates

按条件搜索候选人。支持自然语言查询和结构化过滤。

```json
{
  "name": "hr_search_candidates",
  "description": "按条件搜索候选人。支持自然语言查询和结构化过滤。",
  "inputSchema": {
    "type": "object",
    "properties": {
      "query": { "type": "string", "description": "搜索查询，如'会Python的计算机专业本科生'" },
      "filters": {
        "type": "object",
        "properties": {
          "min_degree": { "type": "string", "description": "最低学历：本科/硕士/博士" },
          "majors": { "type": "array", "items": { "type": "string" }, "description": "专业列表" },
          "skills": { "type": "array", "items": { "type": "string" }, "description": "技能列表" }
        }
      },
      "top_k": { "type": "integer", "description": "返回结果数", "default": 5 }
    },
    "required": ["query"]
  }
}
```

返回：候选人列表 + 匹配理由

### hr_match_jd

根据岗位 JD 匹配候选人。

```json
{
  "name": "hr_match_jd",
  "description": "根据岗位 JD 匹配候选人。自动解析 JD 需求，在简历库中搜索评分排序，返回最匹配的候选人。",
  "inputSchema": {
    "type": "object",
    "properties": {
      "jd_text": { "type": "string", "description": "岗位 JD 全文" },
      "top_k": { "type": "integer", "description": "返回候选人数量", "default": 5 }
    },
    "required": ["jd_text"]
  }
}
```

返回：Top N MatchResult 列表

### C++ 实现方式

C++ 端 `ToolHandler` 回调不做业务逻辑，只负责：
1. 校验参数格式
2. 将请求序列化为 JSON 发给 Python 端
3. 返回 Python 端的处理结果

实际业务逻辑全在 Python 端的 `agent/hr/` 模块中。

## Python HR 模块

### 目录结构

```
agent/hr/
  __init__.py
  types.py              # Pydantic 数据模型
  config.py             # HR 模块配置
  parser/
    __init__.py
    base.py             # BaseParser 抽象类
    resume_parser.py    # ResumeParser：PDF 提取 → LLM 提取结构化信息
    jd_parser.py        # JdParser：JD 文本 → JdRequirements
  store/
    __init__.py
    base.py             # BaseStore 抽象类
    resume_store.py     # ResumeStore：SQLite + ChromaDB 双存储
    jd_store.py         # JdStore：SQLite 存储
  matcher/
    __init__.py
    engine.py           # MatcherEngine：结构化匹配 + 语义匹配 + 综合评分
    scorer.py           # Scorer：各维度评分算法
  engine.py             # HREngine：统一入口
```

### ResumeParser 流程

1. `pdfplumber` 提取 PDF 全文
2. 文本 + 预设 Prompt 发给 LLM（qwen2.5:7b），要求输出 JSON
3. 校验 LLM 输出 JSON 格式，解析失败重试一次
4. 返回 `Resume` 对象

### JdParser 流程

1. JD 文本 + Prompt 发给 LLM
2. 提取 `JdRequirements`
3. 返回 `JobDescription` 对象

### MatcherEngine 流程

1. **结构化筛选**：JD 硬条件（学历、专业）在 SQLite 中 SQL 过滤
2. **语义搜索**：JD 技能需求做向量搜索，在 ChromaDB 中找相关简历
3. **评分**：
   - 教育分：学历匹配度 + 专业相关度
   - 技能分：必备技能重叠度 + 语义相似度
   - 综合分 = 教育分 × 0.4 + 技能分 × 0.6
4. **LLM 分析**：对 Top 候选人生成匹配分析文字

### ResumeStore 存储方案

- **SQLite** 表 `resumes`：id, file_name, name, phone, email, education_json, skills_json, experience_json, projects_json, raw_text, parsed_at
- **ChromaDB** collection `hr_resumes`：简历全文向量，metadata 含 id 和 name
- 导入时同时写入两处
- 查询时 SQLite 做精确筛选，ChromaDB 做语义搜索

### JdStore 存储方案

- **SQLite** 表 `jds`：id, title, raw_text, requirements_json, created_at

## LLM Prompt 设计

### 简历解析 Prompt

```
你是一个专业的简历解析助手。请从以下简历文本中提取结构化信息，严格按 JSON 格式输出。

输出格式：
{
  "name": "姓名",
  "phone": "电话（如有）",
  "email": "邮箱（如有）",
  "age": 年龄数字或null,
  "education": [
    {
      "school": "学校全称",
      "degree": "本科/硕士/博士/大专/其他",
      "major": "专业",
      "gpa": "GPA（如有）",
      "start_date": "入学时间",
      "end_date": "毕业时间"
    }
  ],
  "skills": ["技能1", "技能2"],
  "experience": [
    {
      "company": "公司/组织名",
      "role": "岗位",
      "duration": "时长",
      "description": "工作内容"
    }
  ],
  "projects": [
    {
      "name": "项目名",
      "role": "角色（如有）",
      "description": "项目描述",
      "tech_stack": ["技术1", "技术2"]
    }
  ]
}

简历文本：
---
{resume_text}
---

请只输出 JSON，不要输出其他内容。
```

### JD 解析 Prompt

```
你是招聘需求分析助手。请从以下岗位描述中提取关键需求，按 JSON 格式输出：

{
  "title": "岗位名称",
  "min_degree": "最低学历要求（本科/硕士/博士/不限）",
  "preferred_majors": ["倾向专业"],
  "required_skills": ["必备技能"],
  "preferred_skills": ["加分技能"],
  "experience_years": 经验年限要求（数字或null）
}

岗位描述：
---
{jd_text}
---

请只输出 JSON，不要输出其他内容。
```

### 匹配分析 Prompt

```
你是 HR 招聘助手。请根据以下信息，分析候选人与岗位的匹配度。

岗位需求：
{jd_requirements_json}

候选人简历摘要：
{resume_summary_json}

请用 2-3 句中文分析匹配情况，包括：优势、不足、建议。不要打分，只做定性分析。
```

## 配置

### agent/config.yaml 新增

```yaml
hr:
  enabled: true
  llm_model: "qwen2.5:7b"
  db_path: "./data/hr.db"
  chroma_collection: "hr_resumes"
  parse_retries: 1
  top_k_default: 5
  score_weights:
    education: 0.4
    skills: 0.6
```

同时将顶层 `llm.model` 改为 `qwen2.5:7b`。

## 错误处理

- **PDF 解析失败**：pdfplumber 无法提取文字时（扫描件），返回提示"该 PDF 无法提取文字，可能是扫描件"
- **LLM 输出格式错误**：JSON 解析失败时重试一次，仍失败则标记为"解析失败"并跳过，不影响批量导入
- **重复导入**：根据 file_name 去重，已存在默认跳过（`force_reparse=true` 除外）
- **空查询**：返回错误提示
- **无匹配结果**：放宽条件重试（去掉专业限制），并提示用户

## 不做的事

- 不做 Web UI（仅终端输出）
- 不做扫描件 PDF OCR（仅支持文字版 PDF）
- 不做多用户/权限管理
- 不做简历模板学习或自动评分标准优化