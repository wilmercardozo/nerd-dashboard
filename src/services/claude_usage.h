#pragma once
#include <stdint.h>
#include "device_config.h"

// Uso/limite de Claude SIN admin key: el agente PC lee el token OAuth de Claude
// Code y reporta el % de utilizacion real (headers unified-ratelimit).
struct ClaudeUsage {
    // ── Limite real Pro/Max (via agente PC) ──────────────────────────
    bool     rate_ok           = false;
    int      rate_5h_pct       = 0;    // % ventana 5h
    uint32_t rate_5h_reset_min = 0;    // min hasta reset 5h
    int      rate_7d_pct       = 0;    // % semanal
    uint32_t rate_7d_reset_min = 0;    // min hasta reset semanal
    char     rate_status[16]   = "";   // allowed | allowed_warning | rejected ...

    // ── Contador Claude.ai por extension (secundario, opcional) ──────
    int      web_count   = 0;
    uint16_t web_limit   = 45;
    uint32_t web_reset_s = 0;
};

namespace ClaudeUsageSvc {
    void begin(Config& cfg);
    void recordWebMsg();
    bool consume(ClaudeUsage& out);
    ClaudeUsage snapshot();
    void configChanged();
}
