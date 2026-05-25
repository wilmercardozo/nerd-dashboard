#pragma once
#include "device_config.h"
#include "claude_status.h"   // ClaudeStatus
#include "claude_usage.h"    // ClaudeUsage
#include "pc_metrics.h"      // PcMetrics
#include "pomodoro.h"        // PomoState / PomoMode
#include "task_store.h"      // Task

// Cliente unico del agente PC: hace UN poll a GET /dashboard (claude + rate +
// pc + pomodoro + tareas) y expone snapshots. El ESP ya no corre logica propia
// de claude/pc/tareas/pomodoro: todo vive en el agente. Esto baja la carga del
// ESP (un request) y evita los timeouts / "sin conexion".
#define DASH_MAX_SERVICES 10
struct ServiceItem {
    char name[28]   = "";
    char status[10] = "unknown";   // up | degraded | down | unknown
    char detail[40] = "";
};

namespace Dash {
    void begin(Config& cfg);
    bool consume();                 // true si hubo poll nuevo (refrescar vistas)
    int          services(ServiceItem* out, int maxN);   // monitores (caidos primero)
    ClaudeUsage  usage();
    PcMetrics    pc();
    PomoState    pomo();            // remaining_s interpolado localmente (timer suave)
    int          tasks(Task* out, int maxN);

    // Controles -> POST al agente (se llaman desde el toque en la tarea UI)
    void pomoStart(const char* mode, const char* taskId);
    void pomoPause();
    void pomoReset();
    void pomoSelect(const char* taskId);
    void taskDone(const char* id, bool done);
}
