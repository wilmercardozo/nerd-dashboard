#pragma once
#include "device_config.h"

// Servidor web async (puerto 80) en modo estacion. Sirve la UI desde LittleFS,
// expone la API REST y las rutas OTA. Arranca ANTES que los servicios para que
// el OTA este siempre disponible (regla #7 de CLAUDE.md).
namespace Web {
    void begin(Config& cfg);   // mDNS + rutas + server.begin() (Config mutable: /api/config)
    bool shouldReboot();       // true tras "Reconfigurar WiFi": main hace ESP.restart()
}
