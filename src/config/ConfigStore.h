#pragma once
#include "device_config.h"

// Persistencia de Config en LittleFS:/config.json + flag de "forzar portal".
namespace ConfigStore {
    bool load(Config& out);   // false si falta archivo o error de parseo
    bool save(const Config& cfg);
    bool exists();
    void erase();

    // Flag "abrir portal en proximo boot" — archivo centinela en LittleFS.
    // Permite re-entrar al portal sin borrar la config WiFi guardada.
    bool isForcePortal();     // tambien limpia el flag si estaba puesto
    void setForcePortal();
}
