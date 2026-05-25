"""
Tareas + Pomodoro persistidos en la PC (no en el ESP).
Se guardan en ~/.nerddashboard/. Thread-safe.
"""
import json
import time
import random
import threading
from pathlib import Path

DATA_DIR = Path.home() / ".nerddashboard"
DATA_DIR.mkdir(exist_ok=True)
TASKS_FILE = DATA_DIR / "tasks.json"
POMO_FILE = DATA_DIR / "pomodoro.json"
MON_FILE = DATA_DIR / "monitors.json"

_lock = threading.RLock()


def _load(path, default):
    try:
        if path.exists():
            return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        pass
    return default


def _save(path, data):
    try:
        path.write_text(json.dumps(data), encoding="utf-8")
    except Exception:
        pass


_tasks = _load(TASKS_FILE, [])
_pomo = _load(POMO_FILE, {
    "mode": "idle", "started_at": 0, "duration_s": 0, "paused": False,
    "paused_remaining": 0, "task_id": "", "completed_today": 0,
    "work_count": 0, "last_day": 0, "history": {},
    "config": {"work_s": 1500, "short_s": 300, "long_s": 900, "long_every": 4},
})


# ── TAREAS ─────────────────────────────────────────────────────────────
def list_tasks():
    with _lock:
        return list(_tasks)


def add_task(title, notes="", priority=1, due=0):
    title = (title or "").strip()
    if not title:
        return None
    t = {
        "id": f"t_{int(time.time())}_{random.randint(0, 65535):04x}",
        "title": title[:80], "notes": (notes or "")[:500],
        "priority": int(priority) if priority in (0, 1, 2) else 1,
        "done": False, "created_at": int(time.time()), "completed_at": 0, "due": int(due or 0),
    }
    with _lock:
        _tasks.append(t)
        _save(TASKS_FILE, _tasks)
    return t


def update_task(tid, fields):
    with _lock:
        for t in _tasks:
            if t["id"] == tid:
                if "done" in fields:
                    t["done"] = bool(fields["done"])
                    t["completed_at"] = int(time.time()) if t["done"] else 0
                for k in ("title", "notes", "priority", "due"):
                    if k in fields and fields[k] is not None:
                        t[k] = fields[k]
                _save(TASKS_FILE, _tasks)
                return True
    return False


def delete_task(tid):
    global _tasks
    with _lock:
        n = len(_tasks)
        _tasks = [t for t in _tasks if t["id"] != tid]
        if len(_tasks) != n:
            _save(TASKS_FILE, _tasks)
            return True
    return False


def reorder_tasks(order):
    with _lock:
        idx = {t["id"]: t for t in _tasks}
        new = [idx[i] for i in order if i in idx]
        for t in _tasks:                       # conservar las no mencionadas al final
            if t["id"] not in order:
                new.append(t)
        _tasks[:] = new
        _save(TASKS_FILE, _tasks)
    return True


# ── POMODORO ───────────────────────────────────────────────────────────
def _dur(mode):
    c = _pomo["config"]
    return {"work": c["work_s"], "short_break": c["short_s"], "long_break": c["long_s"]}.get(mode, 0)


def _remaining():
    if _pomo["mode"] == "idle":
        return 0
    if _pomo["paused"]:
        return _pomo["paused_remaining"]
    return max(0, int(_pomo["started_at"] + _pomo["duration_s"] - time.time()))


def _check_advance():
    today = int(time.time() // 86400)
    if today > 0 and _pomo.get("last_day", 0) != today:
        _pomo["last_day"] = today
        _pomo["completed_today"] = 0
    if _pomo["mode"] != "idle" and not _pomo["paused"]:
        if time.time() >= _pomo["started_at"] + _pomo["duration_s"]:
            if _pomo["mode"] == "work":
                _pomo["completed_today"] += 1
                _pomo["work_count"] += 1
                d = time.strftime("%Y-%m-%d")
                _pomo["history"][d] = _pomo["history"].get(d, 0) + 1
                le = _pomo["config"]["long_every"]
                nxt = "long_break" if le > 0 and _pomo["work_count"] % le == 0 else "short_break"
                _pomo["mode"] = nxt
                _pomo["duration_s"] = _dur(nxt)
                _pomo["started_at"] = int(time.time())
            else:
                _pomo["mode"] = "idle"
                _pomo["duration_s"] = 0
            _save(POMO_FILE, _pomo)


def pomo_state():
    with _lock:
        _check_advance()
        title = ""
        for t in _tasks:
            if t["id"] == _pomo["task_id"]:
                title = t["title"]
                break
        return {
            "mode": _pomo["mode"], "remaining_s": _remaining(), "duration_s": _pomo["duration_s"],
            "paused": _pomo["paused"], "current_task_id": _pomo["task_id"],
            "current_task_title": title,
            "completed_today": _pomo["completed_today"], "config": dict(_pomo["config"]),
        }


def pomo_start(mode, task_id=None):
    with _lock:
        _pomo["mode"] = mode if mode in ("work", "short_break", "long_break") else "work"
        _pomo["duration_s"] = _dur(_pomo["mode"])
        _pomo["started_at"] = int(time.time())
        _pomo["paused"] = False
        _pomo["paused_remaining"] = 0
        if task_id is not None:
            _pomo["task_id"] = task_id
        _save(POMO_FILE, _pomo)


def pomo_pause():
    with _lock:
        if _pomo["mode"] != "idle" and not _pomo["paused"]:
            _pomo["paused_remaining"] = _remaining()
            _pomo["paused"] = True
            _save(POMO_FILE, _pomo)
        elif _pomo["paused"]:
            _pomo["started_at"] = int(time.time())
            _pomo["duration_s"] = _pomo["paused_remaining"]
            _pomo["paused"] = False
            _save(POMO_FILE, _pomo)


def pomo_select(task_id):
    # selecciona la tarea actual sin iniciar/cambiar el timer
    with _lock:
        _pomo["task_id"] = task_id or ""
        _save(POMO_FILE, _pomo)


def pomo_reset():
    with _lock:
        _pomo["mode"] = "idle"
        _pomo["duration_s"] = 0
        _pomo["paused"] = False
        _pomo["task_id"] = ""
        _save(POMO_FILE, _pomo)


def pomo_config(work_s=None, short_s=None, long_s=None, long_every=None):
    with _lock:
        c = _pomo["config"]
        if work_s:
            c["work_s"] = int(work_s)
        if short_s:
            c["short_s"] = int(short_s)
        if long_s:
            c["long_s"] = int(long_s)
        if long_every:
            c["long_every"] = int(long_every)
        _save(POMO_FILE, _pomo)


def metrics_summary():
    with _lock:
        pend = sum(1 for t in _tasks if not t["done"])
        done = sum(1 for t in _tasks if t["done"])
        hist = dict(_pomo.get("history", {}))
        return {
            "tasks_pending": pend, "tasks_done": done,
            "pomo_today": _pomo["completed_today"], "pomo_history": hist,
        }


# ── MONITORES (estilo uptime-kuma) ─────────────────────────────────────
# type "statuspage": GET a un status.json de Statuspage -> lee status.indicator
# type "http":       GET a una URL -> up si responde (codigo < 500)
CATALOG = [
    {"name": "Claude",       "type": "statuspage", "url": "https://status.claude.com/api/v2/status.json"},
    {"name": "OpenAI",       "type": "statuspage", "url": "https://status.openai.com/api/v2/status.json"},
    {"name": "Gemini",       "type": "http",       "url": "https://aistudio.google.com/"},
    {"name": "GitHub",       "type": "statuspage", "url": "https://www.githubstatus.com/api/v2/status.json"},
    {"name": "Cloudflare",   "type": "statuspage", "url": "https://www.cloudflarestatus.com/api/v2/status.json"},
    {"name": "Google Cloud", "type": "http",       "url": "https://status.cloud.google.com/"},
    {"name": "AWS",          "type": "http",       "url": "https://health.aws.amazon.com/health/status"},
    {"name": "Vercel",       "type": "statuspage", "url": "https://www.vercel-status.com/api/v2/status.json"},
    {"name": "Netlify",      "type": "statuspage", "url": "https://www.netlifystatus.com/api/v2/status.json"},
    {"name": "Stripe",       "type": "statuspage", "url": "https://status.stripe.com/api/v2/status.json"},
    {"name": "Slack",        "type": "statuspage", "url": "https://status.slack.com/api/v2/status.json"},
    {"name": "Notion",       "type": "statuspage", "url": "https://status.notion.so/api/v2/status.json"},
    {"name": "DigitalOcean", "type": "statuspage", "url": "https://status.digitalocean.com/api/v2/status.json"},
    {"name": "npm",          "type": "statuspage", "url": "https://status.npmjs.org/api/v2/status.json"},
    {"name": "VTEX",         "type": "http",       "url": "https://status.vtex.com/"},
    {"name": "Discord",      "type": "statuspage", "url": "https://discordstatus.com/api/v2/status.json"},
]

_monitors = _load(MON_FILE, None)
if _monitors is None:
    # por defecto: Claude activo
    _monitors = [{
        "id": "m_claude", "name": "Claude", "type": "statuspage",
        "url": "https://status.claude.com/api/v2/status.json",
        "method": "GET", "headers": {}, "enabled": True,
    }]
    _save(MON_FILE, _monitors)

_results = {}   # id -> {status, code, latency_ms, detail, last_check}


def monitor_catalog():
    return CATALOG


def list_monitors():
    # config + ultimo resultado fusionados
    with _lock:
        out = []
        for m in _monitors:
            r = _results.get(m["id"], {})
            out.append({**m, "result": r})
        return out


def monitors_to_poll():
    with _lock:
        return [dict(m) for m in _monitors if m.get("enabled", True)]


def set_result(mid, res):
    with _lock:
        _results[mid] = res


def add_monitor(name, url, type_="http", method="GET", headers=None):
    name = (name or "").strip()
    url = (url or "").strip()
    if not name or not url:
        return None
    m = {
        "id": f"m_{int(time.time())}_{random.randint(0, 65535):04x}",
        "name": name[:32], "type": type_ if type_ in ("http", "statuspage") else "http",
        "url": url, "method": method or "GET", "headers": headers or {}, "enabled": True,
    }
    with _lock:
        _monitors.append(m)
        _save(MON_FILE, _monitors)
    return m


def update_monitor(mid, fields):
    with _lock:
        for m in _monitors:
            if m["id"] == mid:
                for k in ("name", "url", "type", "method", "headers", "enabled"):
                    if k in fields and fields[k] is not None:
                        m[k] = fields[k]
                _save(MON_FILE, _monitors)
                return True
    return False


def delete_monitor(mid):
    global _monitors
    with _lock:
        n = len(_monitors)
        _monitors = [m for m in _monitors if m["id"] != mid]
        _results.pop(mid, None)
        if len(_monitors) != n:
            _save(MON_FILE, _monitors)
            return True
    return False


def services_brief():
    # para /dashboard: lista compacta ordenada (caidos/degradados primero)
    rank = {"down": 0, "degraded": 1, "unknown": 2, "up": 3}
    items = []
    with _lock:
        for m in _monitors:
            if not m.get("enabled", True):
                continue
            r = _results.get(m["id"], {})
            items.append({
                "name": m["name"],
                "status": r.get("status", "unknown"),
                "detail": r.get("detail", ""),
                "latency_ms": r.get("latency_ms", 0),
            })
    items.sort(key=lambda x: (rank.get(x["status"], 2), x["name"].lower()))
    return items
