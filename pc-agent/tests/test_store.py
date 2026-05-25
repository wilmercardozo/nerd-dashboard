"""Tests de store.py: tareas, pomodoro y monitores."""
import store


# ── tareas ──────────────────────────────────────────────────────────────
def test_add_and_list():
    t = store.add_task("Comprar pan", notes="integral", priority=2)
    assert t is not None
    assert t["title"] == "Comprar pan"
    assert t["priority"] == 2
    assert t["done"] is False
    assert store.list_tasks() == [t]


def test_add_empty_title_returns_none():
    assert store.add_task("   ") is None
    assert store.list_tasks() == []


def test_add_clamps_priority_and_lengths():
    t = store.add_task("x" * 200, notes="y" * 1000, priority=99)
    assert len(t["title"]) == 80
    assert len(t["notes"]) == 500
    assert t["priority"] == 1   # 99 no está en (0,1,2) -> default 1


def test_update_done_sets_completed_at():
    t = store.add_task("tarea")
    assert store.update_task(t["id"], {"done": True}) is True
    updated = store.list_tasks()[0]
    assert updated["done"] is True
    assert updated["completed_at"] > 0
    # desmarcar limpia completed_at
    store.update_task(t["id"], {"done": False})
    assert store.list_tasks()[0]["completed_at"] == 0


def test_update_unknown_id_returns_false():
    assert store.update_task("nope", {"done": True}) is False


def test_delete():
    t = store.add_task("borrame")
    assert store.delete_task(t["id"]) is True
    assert store.list_tasks() == []
    assert store.delete_task(t["id"]) is False   # ya no existe


def test_reorder_keeps_unmentioned_at_end():
    a = store.add_task("a")
    b = store.add_task("b")
    c = store.add_task("c")
    store.reorder_tasks([c["id"], a["id"]])   # b no mencionada
    ids = [t["id"] for t in store.list_tasks()]
    assert ids == [c["id"], a["id"], b["id"]]


# ── pomodoro ────────────────────────────────────────────────────────────
def test_pomo_start_sets_running_state():
    store.pomo_start("work")
    s = store.pomo_state()
    assert s["mode"] == "work"
    assert s["paused"] is False
    assert s["duration_s"] == 1500
    assert 1490 <= s["remaining_s"] <= 1500


def test_pomo_pause_freezes_remaining_then_resumes():
    store.pomo_start("work")
    store.pomo_pause()
    s = store.pomo_state()
    assert s["paused"] is True
    rem = s["remaining_s"]
    assert store.pomo_state()["remaining_s"] == rem   # no decrece pausado
    store.pomo_pause()   # toggle -> reanuda
    assert store.pomo_state()["paused"] is False


def test_pomo_reset_clears():
    store.pomo_start("work", task_id="t_1")
    store.pomo_reset()
    s = store.pomo_state()
    assert s["mode"] == "idle"
    assert s["duration_s"] == 0
    assert s["current_task_id"] == ""


def test_pomo_config_updates_durations():
    store.pomo_config(work_s=60, short_s=10)
    store.pomo_start("work")
    assert store.pomo_state()["duration_s"] == 60


def test_pomo_select_sets_current_task_title():
    t = store.add_task("foco")
    store.pomo_select(t["id"])
    assert store.pomo_state()["current_task_title"] == "foco"


# ── monitores ─────────────────────────────────────────────────────────────
def test_add_monitor_validates():
    assert store.add_monitor("", "https://x") is None
    assert store.add_monitor("name", "") is None
    m = store.add_monitor("Mi API", "https://api.example.com", type_="http")
    assert m["enabled"] is True
    assert m["type"] == "http"


def test_update_and_delete_monitor():
    m = store.add_monitor("X", "https://x.com")
    assert store.update_monitor(m["id"], {"enabled": False}) is True
    found = [x for x in store.list_monitors() if x["id"] == m["id"]][0]
    assert found["enabled"] is False
    assert store.delete_monitor(m["id"]) is True
    assert store.delete_monitor(m["id"]) is False


def test_monitors_to_poll_skips_disabled():
    m = store.add_monitor("Off", "https://off.com")
    store.update_monitor(m["id"], {"enabled": False})
    ids = [x["id"] for x in store.monitors_to_poll()]
    assert m["id"] not in ids


def test_services_brief_sorts_down_first():
    a = store.add_monitor("Aaa", "https://a")
    b = store.add_monitor("Bbb", "https://b")
    c = store.add_monitor("Ccc", "https://c")
    store.set_result(a["id"], {"status": "up"})
    store.set_result(b["id"], {"status": "down"})
    store.set_result(c["id"], {"status": "degraded"})
    # m_claude default queda "unknown"
    order = [s["status"] for s in store.services_brief()]
    # down(0) < degraded(1) < unknown(2) < up(3)
    assert order[0] == "down"
    assert order[1] == "degraded"
    assert order[-1] == "up"


def test_metrics_summary_counts():
    store.add_task("pendiente")
    done = store.add_task("hecha")
    store.update_task(done["id"], {"done": True})
    s = store.metrics_summary()
    assert s["tasks_pending"] == 1
    assert s["tasks_done"] == 1
