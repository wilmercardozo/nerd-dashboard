# Contribuir a Nerd Dashboard

Gracias por el interés. Esto es un proyecto hobby — issues y PRs bienvenidos.

## Estructura del repo

- `src/` — firmware ESP32 (C++ / LVGL 9.2.2 / TFT_eSPI). Solo display + touch.
- `pc-agent/` — agente Python (el "cerebro": monitores, uso Claude, métricas, tareas).
- `browser-ext/` — extensión Chrome MV3 (opcional, secundaria).
- `data/` — stub web del ESP (la web real vive en el agente).

Ver el README para la arquitectura completa (ESP = display fino, PC = cerebro).

## Setup de desarrollo

### Firmware

```bash
pio run -e nerd-dashboard          # compila
pio run -e nerd-dashboard -t upload  # flashea por USB
pio device monitor -b 115200       # logs
```

Las flags del panel CYD están en `platformio.ini` (probadas en placa — no toques sin
una placa para verificar). No hace falta `secrets.h` para compilar; la config WiFi/IP
se carga por el portal cautivo al primer arranque.

### Agente PC

```bash
cd pc-agent
pip install -r requirements.txt
python agent.py        # sirve en http://<ip>:8765
```

Tests (no tocan tu `~/.nerddashboard` — usan un tmp dir):

```bash
cd pc-agent
pip install -r requirements-dev.txt
pytest -q
```

## CI

Cada push/PR corre [`.github/workflows/ci.yml`](.github/workflows/ci.yml):

- **firmware** — `pio run -e nerd-dashboard` debe compilar.
- **agent** — `py_compile` + `pytest` sobre `pc-agent/`.

Tu PR debe pasar CI en verde. Si tocas el firmware y no tienes placa, al menos
asegura que compila.

## Convenciones

- Ramas desde `main`. Un PR = un cambio acotado.
- Commits: estilo conciso, en presente (ej. `ui: gauge de tokens`).
- Mantén el firmware liviano: el Flash está ~80% del slot OTA. Buffers grandes al
  HEAP, no `static`. Evita rutas regex en el web server (cuesta ~300KB flash).
- Lógica pesada va al agente Python, no al ESP.

## Reportar bugs

Abre un issue con: qué placa, logs del `pio device monitor`, y/o salida de
`python agent.py`. Captura de pantalla si es visual.
