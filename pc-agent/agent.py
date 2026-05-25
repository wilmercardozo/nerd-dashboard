#!/usr/bin/env python3
"""
Nerd Dashboard — agente PC.

Sirve metricas locales al ESP32 en http://0.0.0.0:8765:
  GET /claude   -> uso de Claude (limite 5h + semanal) leido SIN admin key,
                   usando el token OAuth de Claude Code y los headers
                   anthropic-ratelimit-unified-*  (metodo estilo Clawdmeter).
  GET /metrics  -> CPU / RAM / host / OS (ampliable en Modulo 3: GPU, temps).
  GET /         -> info basica.

El token NUNCA sale de la PC; el agente solo expone porcentajes ya calculados.
Escucha en 0.0.0.0 para que el ESP lo alcance por la LAN (red local).
"""

import os
import json
import time
import threading
import platform
import getpass
import subprocess
import urllib.request
import urllib.error
from pathlib import Path
from urllib.parse import urlparse, parse_qs
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import store

try:
    import psutil
except ImportError:
    psutil = None

# ── configuracion ───────────────────────────────────────────────────────
PORT = 8765
CLAUDE_POLL_S = 60            # limite Anthropic: 1 req/min recomendado
METRICS_POLL_S = 1
KEYCHAIN_SERVICE = "Claude Code-credentials"
CREDENTIALS_PATH = Path.home() / ".claude" / ".credentials.json"
API_URL = "https://api.anthropic.com/v1/messages"
API_BODY = json.dumps({
    "model": "claude-haiku-4-5-20251001",
    "max_tokens": 1,
    "messages": [{"role": "user", "content": "hi"}],
}).encode()

# ── estado compartido ─────────────────────────────────────────────────────
_latest_claude = {"ok": False, "error": "iniciando"}
_latest_metrics = {}
_latest_status = {"ok": False, "error": "iniciando"}
_lock = threading.Lock()

STATUS_URL = "https://status.claude.com/api/v2/summary.json"
MONITOR_POLL_S = 30

# Token de acceso opcional. Si esta seteado, todas las rutas de API exigen
# header "X-Token". Fuente: ~/.nerddashboard/token.txt o env NERD_TOKEN.
def _read_token():
    try:
        f = Path.home() / ".nerddashboard" / "token.txt"
        if f.exists():
            return f.read_text(encoding="utf-8").strip()
    except Exception:
        pass
    return os.environ.get("NERD_TOKEN", "").strip()

TOKEN = _read_token()


# ── lectura del token OAuth de Claude Code ─────────────────────────────────
def _extract_access_token(raw: str):
    try:
        d = json.loads(raw)
    except Exception:
        return None
    o = d.get("claudeAiOauth") or d.get("claude_ai_oauth") or d
    if isinstance(o, dict):
        return o.get("accessToken") or o.get("access_token")
    return None


def read_token():
    # macOS: Keychain
    if platform.system() == "Darwin":
        try:
            out = subprocess.run(
                ["security", "find-generic-password",
                 "-s", KEYCHAIN_SERVICE, "-a", getpass.getuser(), "-w"],
                capture_output=True, text=True, timeout=10,
            )
            t = _extract_access_token(out.stdout) or (out.stdout.strip() or None)
            if t:
                return t
        except Exception:
            pass
    # Windows / Linux (y fallback macOS): archivo de credenciales
    try:
        if CREDENTIALS_PATH.exists():
            return _extract_access_token(CREDENTIALS_PATH.read_text())
    except Exception:
        pass
    return None


# ── parseo de los headers de rate-limit ────────────────────────────────────
def _pct(val):
    if val is None:
        return 0
    try:
        f = float(val)
        return round(f * 100) if f <= 1.0 else round(f)
    except ValueError:
        return 0


def _reset_minutes(val):
    if not val:
        return 0
    try:
        f = float(val)
        if f > 1e9:                       # epoch (segundos)
            return max(0, int((f - time.time()) / 60))
        return max(0, int(f / 60))        # segundos restantes
    except ValueError:
        try:
            from datetime import datetime
            dt = datetime.fromisoformat(val.replace("Z", "+00:00"))
            return max(0, int((dt.timestamp() - time.time()) / 60))
        except Exception:
            return 0


def _build_from_headers(h):
    g = lambda k, d=None: h.get(k, d)
    return {
        "ok": True,
        "s":  _pct(g("anthropic-ratelimit-unified-5h-utilization")),
        "sr": _reset_minutes(g("anthropic-ratelimit-unified-5h-reset")),
        "w":  _pct(g("anthropic-ratelimit-unified-7d-utilization")),
        "wr": _reset_minutes(g("anthropic-ratelimit-unified-7d-reset")),
        "st": g("anthropic-ratelimit-unified-5h-status", "unknown"),
    }


def fetch_claude():
    token = read_token()
    if not token:
        return {"ok": False, "error": "Sin token (Claude Code no logueado?)"}
    req = urllib.request.Request(API_URL, data=API_BODY, method="POST", headers={
        "anthropic-version": "2023-06-01",
        "anthropic-beta": "oauth-2025-04-20",
        "Content-Type": "application/json",
        "User-Agent": "claude-code/2.1.5",
        "Authorization": f"Bearer {token}",
    })
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            return _build_from_headers(r.headers)
    except urllib.error.HTTPError as e:
        # En 429 los headers de rate-limit siguen presentes
        if e.headers and e.headers.get("anthropic-ratelimit-unified-5h-utilization") is not None:
            d = _build_from_headers(e.headers)
            d["http"] = e.code
            return d
        return {"ok": False, "error": f"HTTP {e.code}"}
    except Exception as e:
        return {"ok": False, "error": str(e)}


_gpu_state = {"init": False, "h": None, "fail": False}


def _gpu():
    if _gpu_state["fail"]:
        return {}
    try:
        import pynvml
        if not _gpu_state["init"]:
            pynvml.nvmlInit()
            _gpu_state["h"] = pynvml.nvmlDeviceGetHandleByIndex(0)
            _gpu_state["init"] = True
        h = _gpu_state["h"]
        u = pynvml.nvmlDeviceGetUtilizationRates(h)
        mem = pynvml.nvmlDeviceGetMemoryInfo(h)
        t = pynvml.nvmlDeviceGetTemperature(h, pynvml.NVML_TEMPERATURE_GPU)
        name = pynvml.nvmlDeviceGetName(h)
        if isinstance(name, bytes):
            name = name.decode(errors="ignore")
        return {"gpu": u.gpu, "gpu_mem": round(mem.used / mem.total * 100), "gpu_temp": t, "gpu_name": name}
    except Exception:
        _gpu_state["fail"] = True   # sin NVIDIA / sin pynvml: dejar de intentar
        return {}


def _cpu_temp():
    if not psutil or not hasattr(psutil, "sensors_temperatures"):
        return None
    try:
        st = psutil.sensors_temperatures()
        for key in ("coretemp", "k10temp", "cpu_thermal", "acpitz"):
            if key in st and st[key]:
                return round(st[key][0].current)
        for arr in st.values():
            if arr:
                return round(arr[0].current)
    except Exception:
        pass
    return None


def _battery():
    if not psutil or not hasattr(psutil, "sensors_battery"):
        return None, None
    try:
        b = psutil.sensors_battery()
        if b is None:
            return None, None
        return round(b.percent), bool(b.power_plugged)
    except Exception:
        return None, None


def fetch_status():
    # Estado de servicios Anthropic (status.claude.com). El TLS lo hace la PC,
    # asi el ESP solo lee HTTP plano (evita -32512 por heap).
    try:
        req = urllib.request.Request(STATUS_URL, headers={"User-Agent": "nerd-dashboard"})
        with urllib.request.urlopen(req, timeout=10) as r:
            d = json.loads(r.read().decode())
        comps = [{"name": c.get("name", ""), "status": c.get("status", "")}
                 for c in d.get("components", [])[:8] if c.get("name")]
        st = d.get("status", {})
        return {"ok": True,
                "indicator": st.get("indicator", "unknown"),
                "description": st.get("description", "Sin datos"),
                "components": comps}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def collect_metrics(net_up=0, net_down=0):
    m = {"host": platform.node(), "os": f"{platform.system()} {platform.release()}"}
    if not psutil:
        m["error"] = "psutil no instalado"
        return m
    m["cpu"] = round(psutil.cpu_percent(interval=None))
    vm = psutil.virtual_memory()
    m["ram"] = round(vm.percent)
    m["mem_total"] = round(vm.total / 1073741824, 1)   # GB
    m["mem_used"] = round(vm.used / 1073741824, 1)
    m["cpu_count"] = psutil.cpu_count(logical=True) or 0
    try:
        fr = psutil.cpu_freq()
        if fr:
            m["cpu_freq"] = round(fr.current)
    except Exception:
        pass
    try:
        m["uptime_s"] = int(time.time() - psutil.boot_time())
    except Exception:
        pass
    try:
        disk_path = "C:\\" if platform.system() == "Windows" else "/"
        m["disk"] = round(psutil.disk_usage(disk_path).percent)
    except Exception:
        pass
    ct = _cpu_temp()
    if ct is not None:
        m["cpu_temp"] = ct
    bp, plug = _battery()
    if bp is not None:
        m["batt"] = bp
        m["batt_plugged"] = plug
    m.update(_gpu())
    m["net_up"] = net_up        # bytes/s
    m["net_down"] = net_down
    return m


# ── hilos de polling ────────────────────────────────────────────────────────
def claude_loop():
    global _latest_claude
    while True:
        result = fetch_claude()
        with _lock:
            _latest_claude = result
        print(f"[claude] {result}", flush=True)
        time.sleep(CLAUDE_POLL_S)


def status_loop():
    global _latest_status
    while True:
        _latest_status = fetch_status()
        time.sleep(60)


def metrics_loop():
    global _latest_metrics
    prev = None
    prev_t = None
    if psutil:
        psutil.cpu_percent(interval=None)  # primer call inicializa el delta
        try:
            n = psutil.net_io_counters()
            prev = (n.bytes_sent, n.bytes_recv)
            prev_t = time.time()
        except Exception:
            pass
    while True:
        up = down = 0
        if psutil and prev is not None:
            try:
                n = psutil.net_io_counters()
                now_t = time.time()
                dt = max(0.001, now_t - prev_t)
                up = int((n.bytes_sent - prev[0]) / dt)
                down = int((n.bytes_recv - prev[1]) / dt)
                prev = (n.bytes_sent, n.bytes_recv)
                prev_t = now_t
            except Exception:
                pass
        m = collect_metrics(up, down)
        with _lock:
            _latest_metrics = m
        time.sleep(METRICS_POLL_S)


# ── monitores (uptime) ──────────────────────────────────────────────────────
def check_monitor(m):
    start = time.time()
    headers = {"User-Agent": "nerd-dashboard-monitor"}
    headers.update(m.get("headers") or {})
    try:
        req = urllib.request.Request(m["url"], method=m.get("method", "GET") or "GET", headers=headers)
        with urllib.request.urlopen(req, timeout=10) as r:
            code = r.status
            lat = int((time.time() - start) * 1000)
            body = r.read(20000)
            if m.get("type") == "statuspage":
                try:
                    d = json.loads(body.decode("utf-8"))
                    ind = d.get("status", {}).get("indicator", "none")
                    desc = d.get("status", {}).get("description", "")
                    st = "up" if ind == "none" else ("degraded" if ind in ("minor", "maintenance") else "down")
                    return {"status": st, "code": code, "latency_ms": lat,
                            "detail": desc or ind, "last_check": int(time.time())}
                except Exception:
                    return {"status": "up", "code": code, "latency_ms": lat,
                            "detail": "OK", "last_check": int(time.time())}
            st = "up" if 200 <= code < 400 else "down"
            return {"status": st, "code": code, "latency_ms": lat,
                    "detail": f"HTTP {code}", "last_check": int(time.time())}
    except urllib.error.HTTPError as e:
        lat = int((time.time() - start) * 1000)
        if m.get("type") == "statuspage":
            return {"status": "down", "code": e.code, "latency_ms": lat,
                    "detail": f"HTTP {e.code}", "last_check": int(time.time())}
        # http: 4xx = alcanzable (up), 5xx = caido
        st = "up" if e.code < 500 else "down"
        return {"status": st, "code": e.code, "latency_ms": lat,
                "detail": f"HTTP {e.code}", "last_check": int(time.time())}
    except Exception as e:
        return {"status": "down", "code": 0, "latency_ms": 0,
                "detail": str(e)[:40], "last_check": int(time.time())}


def monitor_loop():
    while True:
        for m in store.monitors_to_poll():
            store.set_result(m["id"], check_monitor(m))
        time.sleep(MONITOR_POLL_S)


# ── servidor HTTP ─────────────────────────────────────────────────────────
WEB_DIR = Path(__file__).resolve().parent / "web"


class Handler(BaseHTTPRequestHandler):
    def _json(self, obj, code=200):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _body(self):
        try:
            n = int(self.headers.get("Content-Length", 0))
            return json.loads(self.rfile.read(n).decode("utf-8")) if n > 0 else {}
        except Exception:
            return {}

    def _authed(self):
        if not TOKEN:
            return True
        return self.headers.get("X-Token", "") == TOKEN

    def _file(self, name, ctype):
        f = WEB_DIR / name
        if not f.exists():
            self._json({"error": "web no encontrada (carpeta pc-agent/web)"}, 404)
            return
        data = f.read_bytes()
        if name == "index.html" and TOKEN:
            data = data.replace(b"__TOKEN__", TOKEN.encode())   # baked-in para el navegador
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        p = urlparse(self.path).path
        if p in ("/", "/index.html"):
            self._file("index.html", "text/html; charset=utf-8"); return
        if p == "/favicon.svg":
            self._file("favicon.svg", "image/svg+xml"); return
        if not self._authed():
            self._json({"error": "unauthorized"}, 401); return
        if p.startswith("/claude_status"):
            self._json(_latest_status)
        elif p.startswith("/claude"):
            with _lock:
                self._json(_latest_claude)
        elif p.startswith("/metrics_summary"):
            self._json(store.metrics_summary())
        elif p.startswith("/metrics"):
            with _lock:
                self._json(_latest_metrics)
        elif p == "/tasks":
            self._json(store.list_tasks())
        elif p == "/pomodoro":
            self._json(store.pomo_state())
        elif p == "/monitors/catalog":
            self._json(store.monitor_catalog())
        elif p == "/monitors":
            self._json(store.list_monitors())
        elif p == "/dashboard":
            # Snapshot combinado para el display del ESP (1 request = todo)
            pend = [t for t in store.list_tasks() if not t["done"]][:8]
            self._json({
                "services": store.services_brief(),
                "rate": _latest_claude,
                "pc": _latest_metrics,
                "pomodoro": store.pomo_state(),
                "tasks": pend,
            })
        else:
            self._json({"error": "not found"}, 404)

    def do_POST(self):
        if not self._authed():
            self._json({"error": "unauthorized"}, 401); return
        p = urlparse(self.path).path
        b = self._body()
        if p == "/tasks":
            t = store.add_task(b.get("title", ""), b.get("notes", ""), b.get("priority", 1), b.get("due", 0))
            self._json(t or {"error": "titulo requerido"}, 201 if t else 400)
        elif p == "/tasks/reorder":
            store.reorder_tasks(b.get("order", []))
            self._json({"ok": True})
        elif p == "/pomodoro/start":
            store.pomo_start(b.get("mode", "work"), b.get("task_id"))
            self._json({"ok": True})
        elif p == "/pomodoro/pause":
            store.pomo_pause()
            self._json({"ok": True})
        elif p == "/pomodoro/reset":
            store.pomo_reset()
            self._json({"ok": True})
        elif p == "/pomodoro/select":
            store.pomo_select(b.get("task_id", ""))
            self._json({"ok": True})
        elif p == "/pomodoro/config":
            store.pomo_config(b.get("work_s"), b.get("short_s"), b.get("long_s"), b.get("long_every"))
            self._json({"ok": True})
        elif p == "/monitors":
            m = store.add_monitor(b.get("name", ""), b.get("url", ""),
                                  b.get("type", "http"), b.get("method", "GET"), b.get("headers"))
            self._json(m or {"error": "name+url requeridos"}, 201 if m else 400)
        else:
            self._json({"error": "not found"}, 404)

    def do_PUT(self):
        if not self._authed():
            self._json({"error": "unauthorized"}, 401); return
        p = urlparse(self.path).path
        b = self._body()
        if p == "/tasks":
            ok = store.update_task(b.get("id", ""), b)
            self._json({"ok": ok}, 200 if ok else 404)
        elif p == "/monitors":
            ok = store.update_monitor(b.get("id", ""), b)
            self._json({"ok": ok}, 200 if ok else 404)
        else:
            self._json({"error": "not found"}, 404)

    def do_DELETE(self):
        if not self._authed():
            self._json({"error": "unauthorized"}, 401); return
        u = urlparse(self.path)
        if u.path == "/tasks":
            tid = parse_qs(u.query).get("id", [""])[0]
            self._json({"ok": store.delete_task(tid)}, 200)
        elif u.path == "/monitors":
            mid = parse_qs(u.query).get("id", [""])[0]
            self._json({"ok": store.delete_monitor(mid)}, 200)
        else:
            self._json({"error": "not found"}, 404)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type,X-Token")
        self.end_headers()

    def log_message(self, *args):
        pass  # silencioso


def main():
    threading.Thread(target=claude_loop, daemon=True).start()
    threading.Thread(target=status_loop, daemon=True).start()
    threading.Thread(target=metrics_loop, daemon=True).start()
    threading.Thread(target=monitor_loop, daemon=True).start()
    if TOKEN:
        print("[agent] token de acceso ACTIVO (X-Token requerido)", flush=True)
    srv = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"[agent] dashboard: http://localhost:{PORT}  (tareas, pomodoro, claude, pc)", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\n[agent] detenido")


if __name__ == "__main__":
    main()
