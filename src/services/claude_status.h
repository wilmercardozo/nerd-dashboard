#pragma once
#include <stdint.h>

#define CLAUDE_MAX_COMPONENTS 8

// Estado de servicios Anthropic (status.anthropic.com /api/v2/summary.json).
struct ClaudeStatus {
    char indicator[16]   = "unknown";   // none|minor|major|critical|maintenance|unknown
    char description[64] = "Sin datos"; // ej: "All Systems Operational"
    uint8_t comp_count   = 0;
    struct {
        char name[40];
        char status[24];                // operational|degraded_performance|partial_outage|major_outage
    } comps[CLAUDE_MAX_COMPONENTS];
    int      last_http      = 0;        // ultimo codigo HTTP (>0) o error (<0)
    uint32_t last_update_ms = 0;        // millis del ultimo fetch OK (0 = nunca)
    bool     valid          = false;
};

struct Config;  // fwd

namespace ClaudeStatusSvc {
    void begin(Config& cfg);            // arranca tarea de polling (cada CLAUDE_STATUS_POLL_MS)
    bool consume(ClaudeStatus& out);    // true si hay datos nuevos desde el ultimo consume()
    ClaudeStatus snapshot();            // copia thread-safe del estado actual
}
