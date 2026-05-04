"""HR module data models."""

from __future__ import annotations

import uuid
from datetime import datetime
from typing import Any, Optional

from pydantic import BaseModel, Field


class Education(BaseModel):
    """Education record."""

    school: str
    degree: str
    major: str
    gpa: Optional[str] = None
    start_date: Optional[str] = None
    end_date: Optional[str] = None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return self.model_dump()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Education:
        """Create from dictionary."""
        return cls(**data)


class Experience(BaseModel):
    """Work experience record."""

    company: str
    role: str
    duration: str
    description: str

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return self.model_dump()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Experience:
        """Create from dictionary."""
        return cls(**data)


class Project(BaseModel):
    """Project record."""

    name: str
    role: Optional[str] = None
    description: str
    tech_stack: list[str] = Field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return self.model_dump()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Project:
        """Create from dictionary."""
        return cls(**data)


class Resume(BaseModel):
    """Resume data model."""

    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    file_name: str
    name: str
    phone: Optional[str] = None
    email: Optional[str] = None
    age: Optional[int] = None
    education: list[Education] = Field(default_factory=list)
    skills: list[str] = Field(default_factory=list)
    experience: list[Experience] = Field(default_factory=list)
    projects: list[Project] = Field(default_factory=list)
    raw_text: str = ""
    parsed_at: datetime = Field(default_factory=datetime.now)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary with ISO datetime."""
        d = self.model_dump()
        d["parsed_at"] = self.parsed_at.isoformat()
        return d

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Resume:
        """Create from dictionary, parsing ISO datetime."""
        if isinstance(data.get("parsed_at"), str):
            data = dict(data)
            data["parsed_at"] = datetime.fromisoformat(data["parsed_at"])
        return cls(**data)


class JdRequirements(BaseModel):
    """Job description requirements."""

    min_degree: Optional[str] = None
    preferred_majors: list[str] = Field(default_factory=list)
    required_skills: list[str] = Field(default_factory=list)
    preferred_skills: list[str] = Field(default_factory=list)
    experience_years: Optional[int] = None

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary."""
        return self.model_dump()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> JdRequirements:
        """Create from dictionary."""
        return cls(**data)


class JobDescription(BaseModel):
    """Job description data model."""

    id: str = Field(default_factory=lambda: str(uuid.uuid4()))
    title: str
    raw_text: str
    requirements: JdRequirements = Field(default_factory=JdRequirements)
    created_at: datetime = Field(default_factory=datetime.now)

    def to_dict(self) -> dict[str, Any]:
        """Convert to dictionary with ISO datetime."""
        d = self.model_dump()
        d["created_at"] = self.created_at.isoformat()
        return d

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> JobDescription:
        """Create from dictionary, parsing ISO datetime."""
        if isinstance(data.get("created_at"), str):
            data = dict(data)
            data["created_at"] = datetime.fromisoformat(data["created_at"])
        return cls(**data)


class MatchResult(BaseModel):
    """Resume-to-JD match result."""

    resume: Resume
    overall_score: float
    education_score: float
    skills_score: float
    match_details: str
