# Extensión Chrome — Nerd Dashboard (contador Claude.ai)

Cuenta los mensajes que envías en `claude.ai` (ventana deslizante de 5h) y los
reporta al ESP32 vía `POST /api/claude/msg`.

## Instalar (modo desarrollador)

1. Chrome → `chrome://extensions`
2. Activar **Modo de desarrollador** (arriba a la derecha).
3. **Cargar descomprimida** → seleccionar esta carpeta `browser-ext/`.
4. Click en el icono de la extensión → escribir la **IP del ESP** (la que sale
   en la barra del display, ej. `192.168.0.126`) → **Guardar IP** → **Probar conexión**.

## Cómo funciona

- `content.js` (en claude.ai) detecta el envío: Enter sin Shift en el compositor,
  o click en el botón Send/Enviar. Es una heurística → conteo **aproximado**.
- `background.js` (service worker) mantiene el contador local y hace el `fetch`
  http al ESP. Se hace en el background —no en la página— porque claude.ai es
  https y bloquearía un fetch http (contenido mixto); el service worker no.
- Si el ESP no responde, encola y reintenta cada minuto (`pending`).

## Notas

- `host_permissions` incluye `http://*/*` para poder postear al ESP en la LAN.
- Sin iconos por simplicidad (Chrome los hace opcionales). Para añadirlos, crear
  `icons/16.png`, `48.png`, `128.png` y declararlos en `manifest.json`.
- Los selectores de claude.ai cambian seguido; si deja de contar, revisar
  `content.js` (aria-label del botón / estructura del compositor).
