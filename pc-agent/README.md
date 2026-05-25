# Agente PC — Nerd Dashboard

Corre en tu PC (Windows/macOS/Linux) y sirve métricas al ESP32 en
`http://0.0.0.0:8765`:

- `GET /claude` — **uso real de Claude (límite 5h + semanal) SIN admin key**.
  Lee el token OAuth de Claude Code y consulta los headers `anthropic-ratelimit-unified-*`.
- `GET /metrics` — CPU / RAM / host / OS (se amplía con GPU/temps en el Módulo 3).

```json
// GET /claude
{"ok":true,"s":7,"sr":234,"w":9,"wr":6924,"st":"allowed"}
//  s=5h %, sr=reset 5h (min), w=semana %, wr=reset semana (min), st=status
```

## Requisitos

- Python 3.10+
- **Claude Code instalado y logueado** en esta PC (el agente lee su token).
  - Linux/Windows: `~/.claude/.credentials.json`
  - macOS: Keychain (`Claude Code-credentials`)
- `pip install -r requirements.txt`

## Correr manualmente

```bash
python agent.py        # Windows
python3 agent.py       # macOS/Linux
```

## Autoarranque al iniciar sesión

```powershell
# Windows
powershell -ExecutionPolicy Bypass -File install-windows.ps1
```
```bash
# macOS
bash install-macos.sh
```

## Conectar con el ESP

En `http://nerddashboard.local` → tarjeta **"Agente PC"** → poner la **IP de esta PC**
(p.ej. `192.168.0.50`) y puerto `8765` → Guardar. El display mostrará el % real.

## Seguridad / notas

- El token **nunca sale de la PC**: el agente solo expone porcentajes ya calculados.
- Escucha en `0.0.0.0` (toda la LAN) para que el ESP lo alcance. Si tu red no es
  confiable, restringe por firewall al IP del ESP.
- Hace 1 request mínima (`max_tokens=1`) a Anthropic por minuto → consumo despreciable.
- Método no oficial (headers `unified-ratelimit`); si Anthropic los cambia, habrá
  que ajustar `agent.py`. Inspirado en [Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter).
