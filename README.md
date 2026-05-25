# Nerd Dashboard

Dashboard de escritorio sobre una placa **Cheap Yellow Display (ESP32-2432S028R)**
+ un **agente en tu PC**. Monitorea servicios (estilo uptime-kuma), tu uso de
Claude, las métricas de tu PC, y gestiona tareas con Pomodoro.

![hardware](https://img.shields.io/badge/board-ESP32--2432S028R%20(CYD)-orange) ![lvgl](https://img.shields.io/badge/UI-LVGL%209.2-blue) ![agent](https://img.shields.io/badge/agent-Python%203.10%2B-green)

---

## Cómo funciona (arquitectura)

El ESP32 es débil para ser un servidor web ocupado, así que la lógica vive en el PC:

```
┌─────────────┐      HTTP (LAN)      ┌──────────────────────────────┐
│  ESP32 CYD  │ ──GET /dashboard──▶  │   Agente PC (Python)         │
│  (display)  │ ◀─JSON (1 req/2s)─   │   = el "cerebro"             │
│             │                      │  • monitor de servicios      │
│  4 vistas   │ ──POST controles──▶  │  • uso Claude (sin admin key)│
│  touch      │                      │  • métricas PC (CPU/RAM/GPU) │
└─────────────┘                      │  • tareas + pomodoro         │
                                     │  • dashboard web :8765       │
   navegador ──────────────────────▶ └──────────────────────────────┘
   (PC/celu, o remoto vía VPN)
```

- **ESP32 = display fino.** Hace 1 request a `/dashboard` cada 2s y dibuja las 4
  vistas. Los toques (pomodoro, seleccionar/terminar tarea) van por POST al agente.
- **Agente PC = cerebro.** Guarda todo en `~/.nerddashboard/`, chequea servicios,
  lee tu token de Claude Code, mide tu PC, y sirve el dashboard web completo.

> El agente **debe** correr en tu PC: usa tu sesión local de Claude Code y mide
> *ese* hardware. No se sube a un servidor cloud para esos datos.

---

## Las 4 vistas del display

| Tab | Contenido |
|-----|-----------|
| **Servicios** | Monitor de uptime: lista de servicios; los caídos/degradados primero, con indicador de color. |
| **Tokens** | Uso real de Claude Pro/Max (ventana 5h + semanal), **sin admin key**. |
| **PC** | CPU (anillo), RAM/GPU/Disco, red, temperaturas, batería, uptime. |
| **Tareas** | Pomodoro (timer) + lista de tareas. Tap = seleccionar, long-press = terminar. |

---

## Requisitos

- **Hardware:** ESP32-2432S028R ("CYD"), cable USB.
- **Firmware:** [PlatformIO Core](https://platformio.org/) (probado con 6.1.19).
- **Agente:** Python 3.10+, y **Claude Code instalado y logueado** en ese PC
  (para el uso de tokens). GPU NVIDIA opcional (`nvidia-ml-py`).

---

## Instalación

### 1. Agente PC (primero)

```bash
cd pc-agent
pip install -r requirements.txt
python agent.py          # Windows: python   ·   macOS/Linux: python3
```

Sirve en `http://<ip-del-pc>:8765`. Autoarranque al iniciar sesión:

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File pc-agent\install-windows.ps1
```
```bash
# macOS
bash pc-agent/install-macos.sh
```

### 2. Firmware (ESP32)

```bash
pio run -e nerd-dashboard -t upload     # compila + flashea por USB
pio device monitor -b 115200            # logs (opcional)
```

Primer arranque → crea el AP **`NerdDashboard-Setup`**. Conéctate, abre
`http://192.168.4.1`, y configura: **WiFi**, nombre, **IP del PC (agente)** y zona horaria.

### 3. Extensión Chrome (opcional)

Cuenta mensajes de `claude.ai`. `chrome://extensions` → modo desarrollador →
*Cargar descomprimida* → carpeta `browser-ext/`. (Secundario; el medidor real de
límite ya viene del agente.)

---

## Uso

- **Dashboard completo:** navegador → `http://<ip-del-pc>:8765`. Ahí gestionas
  servicios, tareas, pomodoro y ves todo (rápido, persistente).
- **Display:** refleja lo mismo en ~2s. Tap en tarea = seleccionar para pomodoro;
  long-press = marcar terminada.
- **Cambiar WiFi:** landing del ESP (`http://nerddashboard.local`) → "Reconfigurar
  WiFi". O si la red guardada no existe al arrancar, entra solo al portal.

### Monitor de servicios

En el dashboard web → panel **Servicios**:
- Agrega del **catálogo** (Claude, OpenAI, Gemini, GitHub, Cloudflare, Google
  Cloud, AWS, Vercel, Netlify, Stripe, Slack, Notion, DigitalOcean, npm, VTEX, Discord).
- O **personalizado**: nombre + URL (GET) + headers (key/value, ej. `Authorization`).
- Tipos: `statuspage` (lee el indicador de un status.json) o `http` (¿responde?).
- Editar (✎), activar/desactivar (✓), borrar (✕).

### Uso de Claude sin admin key

El agente lee el token OAuth de Claude Code (`~/.claude/.credentials.json` o
Keychain en macOS) y consulta los headers `anthropic-ratelimit-unified-*` →
% real de tu ventana de 5h y semanal. No necesita Admin API.

---

## Seguridad

- Por defecto el agente escucha en la LAN **sin autenticación** (red local).
- **Token opcional:** crea `~/.nerddashboard/token.txt` (o env `NERD_TOKEN`) y
  reinicia el agente → toda la API exige header `X-Token`. Pon el mismo token en
  la landing del ESP. El navegador del dashboard lo recibe embebido.
- **Acceso remoto:** usa **Tailscale/WireGuard** o Cloudflare Tunnel — nunca abras
  el puerto directo a internet. El display se queda en la LAN (no hace TLS).

---

## API del agente (referencia)

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | `/dashboard` | Snapshot combinado para el ESP |
| GET | `/claude` | Uso 5h/semanal (ratelimit) |
| GET | `/metrics` · `/metrics_summary` | Métricas PC · resumen tareas/pomodoro |
| GET/POST/PUT/DELETE | `/monitors` | Monitores (CRUD) |
| GET | `/monitors/catalog` | Catálogo predefinido |
| GET/POST | `/tasks` · `/tasks/reorder` | Tareas (listar/crear/reordenar) |
| PUT/DELETE | `/tasks` · `/tasks?id=` | Editar/terminar · borrar |
| GET/POST | `/pomodoro` · `/pomodoro/{start,pause,reset,select,config}` | Pomodoro |

---

## Estructura

```
platformio.ini            env PlatformIO (LVGL 9.2.2, TFT_eSPI CYD)
partitions_custom.csv     dual-OTA + LittleFS
include/                  lv_conf.h, device_config.h (struct Config)
src/
  config.h                #defines globales
  config/                 ConfigStore (LittleFS)
  network/                WiFiManager
  web/                    server (landing+OTA+config), portal (AP), ota
  ui/                     ui_manager (tabview+touch) + 4 vistas
  services/dash.{h,cpp}   cliente único del agente (poll /dashboard + POST)
pc-agent/
  agent.py                servidor HTTP + pollers (claude/status/metrics/monitor)
  store.py                tareas, pomodoro, monitores (persistencia JSON)
  web/index.html          dashboard web completo
  install-*.{ps1,sh}      autoarranque
browser-ext/              extensión Chrome (MV3)
data/                     stub web del ESP (no se usa; web vive en el agente)
```

## Troubleshooting

- **"Agente PC offline" en el display:** el agente no corre o IP mal. Revisa
  `python agent.py` activo y la IP en la landing del ESP.
- **Backlight apagado:** probar `-DTFT_BL=27` en `platformio.ini`.
- **Colores invertidos:** quitar `-DTFT_INVERSION_ON`.
- **GPU n/a:** instala `pip install nvidia-ml-py` (solo NVIDIA).
- **OTA:** `http://nerddashboard.local/update` (solo firmware; el agente no se flashea).

---

Hecho con ESP32 + LVGL + Python. Inspirado en parte por
[Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) para el método de uso de Claude.
