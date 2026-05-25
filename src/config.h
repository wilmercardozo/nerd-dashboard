#pragma once
// Constantes globales del firmware Nerd Dashboard (CYD / ESP32-2432S028R).

// ── Version ───────────────────────────────────────────────────────────
#define FW_VERSION    "0.4.0"          // Modulos 1-4 (Claude, Tokens, PC, Pomodoro+Tareas)
#define DEVICE_MODEL  "ESP32-2432S028R"

// ── Portal cautivo / red ──────────────────────────────────────────────
#define AP_SSID       "NerdDashboard-Setup"
#define MDNS_NAME     "nerddashboard"  // accesible en http://nerddashboard.local

// ── Pines touch XPT2046 (bus VSPI, separado del TFT en HSPI) ───────────
#define TOUCH_CS      33
#define TOUCH_IRQ     36
#define TOUCH_SCK     25
#define TOUCH_MISO    39
#define TOUCH_MOSI    32

// Calibracion touch: raw XPT2046 -> pixeles, rotacion 3 (ambos ejes invertidos)
#define TOUCH_RAW_MIN 200
#define TOUCH_RAW_MAX 3900

// ── Display ────────────────────────────────────────────────────────────
#define SCREEN_W      320
#define SCREEN_H      240
#define UI_TARGET_FPS 30

// ── Intervalos de polling (ms) ─────────────────────────────────────────
#define CLAUDE_STATUS_POLL_MS  60000UL    // status.claude.com cada 60s
#define ADMIN_API_POLL_MS      300000UL   // Admin API cada 5 min (limite: 1/min)

// ── Contador Claude.ai (ventana deslizante de 5h) ──────────────────────
#define CLAUDE_WEB_WINDOW_S    18000UL    // 5 horas en segundos
#define CLAUDE_MSGS_MAX        300        // tope de timestamps en buffer/archivo

// ── Admin API (uso + costo) ────────────────────────────────────────────
#define ADMIN_API_HOST         "api.anthropic.com"
#define ANTHROPIC_VERSION      "2023-06-01"

// ── Tareas + Pomodoro (Modulo 4) ───────────────────────────────────────
#define TASKS_MAX              40          // tope de tareas en RAM/archivo (heap)
#define TASK_TITLE_MAX         81
#define TASK_NOTES_MAX         120
#define TASK_ID_MAX            24

// ── Servicios externos ─────────────────────────────────────────────────
// Anthropic movio el status a claude.com; status.anthropic.com -> 302 -> claude.com
#define CLAUDE_STATUS_HOST "status.claude.com"
#define CLAUDE_STATUS_PATH "/api/v2/summary.json"
