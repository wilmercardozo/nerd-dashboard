#pragma once
#include <stdint.h>
#include "device_config.h"

// Métricas del PC del usuario, obtenidas del agente (GET /metrics).
// Campos con valor -1 = no disponibles en ese SO/hardware.
struct PcMetrics {
    bool     online   = false;
    char     host[40] = "";
    char     os[40]   = "";
    int      cpu      = 0;    // %
    int      ram      = 0;    // %
    int      disk     = -1;   // %
    int      gpu      = -1;   // %
    int      gpu_mem  = -1;   // %
    int      cpu_temp = -1;   // C
    int      gpu_temp = -1;   // C
    int      batt     = -1;   // %
    bool     batt_plugged = false;
    uint32_t net_up   = 0;    // bytes/s
    uint32_t net_down = 0;    // bytes/s
    int      cpu_count = -1;  // nucleos logicos
    int      cpu_freq  = -1;  // MHz
    float    mem_total = 0;   // GB
    float    mem_used  = 0;   // GB
    uint32_t pc_uptime_s = 0; // uptime del PC
    char     gpu_name[40] = "";
    uint32_t age_s    = 0;    // antiguedad del ultimo dato OK
};

namespace PcMetricsSvc {
    void begin(Config& cfg);
    bool consume(PcMetrics& out);
    PcMetrics snapshot();
}
