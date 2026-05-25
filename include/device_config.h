#pragma once
#include <stdint.h>

// Configuracion persistente del dispositivo (LittleFS:/config.json).
// Struct plano de arrays de char fijos (sin String) — serializable sin heap.
// Campos de modulos futuros incluidos desde ya para mantener estable el
// esquema JSON entre versiones.
//
// NOTA: este archivo se llama device_config.h (no Config.h) para evitar
// colision case-insensitive con src/config.h en Windows.
struct Config {
    // ── WiFi (Modulo 1) ──────────────────────────────────────────────
    char     wifi_ssid[64]      = "";
    char     wifi_pass[64]      = "";
    char     device_name[32]    = "nerddashboard";   // hostname mDNS
    int8_t   timezone_offset    = -5;                 // horas respecto a UTC

    // ── Tokens / uso Claude (Modulo 2) ───────────────────────────────
    char     admin_key[160]     = "";    // Anthropic Admin API key (sk-ant-admin...)
    float    budget_monthly_usd = 30.0f; // presupuesto mensual API
    uint16_t web_msg_limit      = 45;    // limite aprox. mensajes ventana 5h

    // ── Agente PC (Modulo 3) ─────────────────────────────────────────
    char     pc_agent_host[64]  = "";    // IP/host del agente Python
    uint16_t pc_agent_port      = 8765;
    uint32_t pc_poll_ms         = 2000;

    // ── Seguridad opcional ───────────────────────────────────────────
    char     api_token[33]      = "";    // si no vacio, exige header X-Token en endpoints sensibles
    char     ota_pass[33]       = "";    // si no vacio, /update exige Basic Auth (user "admin")

    bool     valid              = false; // true tras cargar/guardar OK
};
