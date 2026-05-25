"""Fixtures de test. Aísla store.py: redirige la persistencia a un tmp dir
para que los tests NUNCA toquen ~/.nerddashboard del usuario."""
import sys
from pathlib import Path

import pytest

# pc-agent en el path para poder `import store` / `import agent`
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import store  # noqa: E402


def _fresh_pomo():
    return {
        "mode": "idle", "started_at": 0, "duration_s": 0, "paused": False,
        "paused_remaining": 0, "task_id": "", "completed_today": 0,
        "work_count": 0, "last_day": 0, "history": {},
        "config": {"work_s": 1500, "short_s": 300, "long_s": 900, "long_every": 4},
    }


def _default_monitor():
    return {
        "id": "m_claude", "name": "Claude", "type": "statuspage",
        "url": "https://status.claude.com/api/v2/status.json",
        "method": "GET", "headers": {}, "enabled": True,
    }


@pytest.fixture(autouse=True)
def isolated_store(tmp_path, monkeypatch):
    # Persistencia a tmp: _save(TASKS_FILE, ...) resuelve el global parcheado.
    monkeypatch.setattr(store, "TASKS_FILE", tmp_path / "tasks.json")
    monkeypatch.setattr(store, "POMO_FILE", tmp_path / "pomodoro.json")
    monkeypatch.setattr(store, "MON_FILE", tmp_path / "monitors.json")
    # Estado en memoria reseteado entre tests.
    store._tasks[:] = []
    store._results.clear()
    store._monitors[:] = [_default_monitor()]
    store._pomo.clear()
    store._pomo.update(_fresh_pomo())
    yield
