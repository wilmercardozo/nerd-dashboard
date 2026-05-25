#pragma once
#include "device_config.h"

namespace WiFiMgr {
    bool connect(const Config& cfg);  // true al conectar; marca needsPortal al fallar
    bool isConnected();
    bool needsPortal();
    void clearPortalFlag();

    // Tarea en segundo plano: monitorea WiFi.status() y reconecta con backoff
    // exponencial (10s -> 30s -> 60s -> 120s) cuando cae el enlace. Idempotente:
    // solo la primera llamada crea la tarea. Pasar la misma Config usada en connect().
    void startAutoReconnect(const Config& cfg);
}
