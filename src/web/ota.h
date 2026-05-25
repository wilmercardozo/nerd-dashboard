#pragma once
#include <ESPAsyncWebServer.h>
#include "device_config.h"

// OTA por web: formulario en GET /update, recepcion multipart en POST /update.
// Siempre disponible (rutas registradas al arrancar el server, antes de los
// servicios) para poder recuperar el dispositivo aunque una vista falle.
//
// Seguridad: si cfg.ota_pass no esta vacio, /update exige HTTP Basic Auth
// (usuario "admin", clave = ota_pass). Vacio = abierto (recuperabilidad).
namespace Ota {
    void registerRoutes(AsyncWebServer& srv, Config& cfg);
    bool shouldReboot();   // true tras un flasheo OK; main hace ESP.restart()
}
