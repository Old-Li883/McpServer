# agent/hr/tests/test_config.py
from agent.config import load_config


def test_hr_config_defaults():
    config = load_config()
    assert config.hr.enabled is True
    assert config.hr.llm_model == "qwen2.5:7b"
    assert config.hr.db_path == "./data/hr.db"
    assert config.hr.chroma_collection == "hr_resumes"
    assert config.hr.parse_retries == 1
    assert config.hr.top_k_default == 5
    assert config.hr.score_weights["education"] == 0.4
    assert config.hr.score_weights["skills"] == 0.6
