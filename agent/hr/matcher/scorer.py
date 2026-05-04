"""Scoring algorithms for resume-JD matching."""

from agent.hr.types import Resume, JdRequirements

DEGREE_ORDER = {"大专": 0, "本科": 1, "硕士": 2, "博士": 3}


class Scorer:
    """Computes education, skills, and overall match scores."""

    def __init__(self, education_weight: float = 0.4, skills_weight: float = 0.6):
        self._edu_w = education_weight
        self._skill_w = skills_weight

    def education_score(self, resume: Resume, req: JdRequirements) -> float:
        """Score 0-100 based on highest degree vs. minimum required degree.

        Args:
            resume: The candidate's resume.
            req: The JD requirements.

        Returns:
            Score between 0.0 and 100.0.
        """
        if not req.min_degree or req.min_degree == "不限":
            return 100.0
        threshold = DEGREE_ORDER.get(req.min_degree, 0)
        max_level = max(
            (DEGREE_ORDER.get(e.degree, 0) for e in resume.education), default=-1
        )
        if max_level >= threshold:
            return 100.0
        gap = threshold - max_level
        return max(0.0, 100.0 - gap * 30.0)

    def skills_score(self, resume: Resume, req: JdRequirements) -> float:
        """Score 0-100 based on required and preferred skill overlap.

        Args:
            resume: The candidate's resume.
            req: The JD requirements.

        Returns:
            Score between 0.0 and 100.0.
        """
        resume_skills_lower = {s.lower() for s in resume.skills}
        required = req.required_skills
        preferred = req.preferred_skills

        if not required and not preferred:
            return 100.0

        required_hit = sum(1 for s in required if s.lower() in resume_skills_lower)
        preferred_hit = sum(1 for s in preferred if s.lower() in resume_skills_lower)

        required_score = (required_hit / len(required) * 80.0) if required else 80.0
        preferred_score = (preferred_hit / len(preferred) * 20.0) if preferred else 20.0
        return min(100.0, required_score + preferred_score)

    def overall_score(self, education_score: float, skills_score: float) -> float:
        """Weighted combination of education and skills scores.

        Args:
            education_score: Education match score (0-100).
            skills_score: Skills match score (0-100).

        Returns:
            Weighted overall score (0-100).
        """
        return education_score * self._edu_w + skills_score * self._skill_w
