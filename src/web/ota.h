#pragma once
#include <ESPAsyncWebServer.h>

// OTA por web: formulario en GET /update, recepcion multipart en POST /update.
// Siempre disponible (rutas registradas al arrancar el server, antes de los
// servicios) para poder recuperar el dispositivo aunque una vista falle.
namespace Ota {
    void registerRoutes(AsyncWebServer& srv);
    bool shouldReboot();   // true tras un flasheo OK; main hace ESP.restart()
}
