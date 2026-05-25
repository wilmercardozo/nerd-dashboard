#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

#include "config.h"
#include "device_config.h"
#include "config/ConfigStore.h"
#include "network/WiFiManager.h"
#include "web/portal.h"
#include "web/server.h"
#include "web/ota.h"
#include "ui/ui_manager.h"
#include "services/dash.h"

// ── estado compartido ──────────────────────────────────────────────────
Config gConfig;
static bool gInPortalMode = false;

// ── tarea UI (core 0, prio 2): touch via indev LVGL + render ────────────
static void taskUI(void*) {
    for (;;) {
        UIManager::tick();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void startUITask() {
    xTaskCreatePinnedToCore(taskUI, "ui", 8192, nullptr, 2, nullptr, 0);
}

void setup() {
    Serial.begin(115200);
    Serial.println("[boot] Nerd Dashboard " FW_VERSION);

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    // Display + UI (la tarea UI aun no corre; setup() rendea con pumpLvgl)
    UIManager::init();
    UIManager::setStatus("Iniciando...");
    UIManager::pumpLvgl(300);

    bool hasConfig   = ConfigStore::load(gConfig);
    bool forcePortal = ConfigStore::isForcePortal();

    // ── Sin config o portal forzado → modo AP de configuracion ──────
    if (!hasConfig || forcePortal) {
        Serial.printf("[boot] Portal (hasConfig=%d forcePortal=%d)\n", hasConfig, forcePortal);
        gInPortalMode = true;
        UIManager::setStatus(forcePortal ? "Portal (solicitado)" : "Sin config - portal");
        UIManager::pumpLvgl(200);
        Portal::start();
        startUITask();
        return;
    }

    // ── Conexion WiFi ───────────────────────────────────────────────
    UIManager::setStatus("Conectando WiFi...");
    UIManager::pumpLvgl(100);
    WiFiMgr::connect(gConfig);

    if (WiFiMgr::needsPortal()) {
        Serial.println("[boot] WiFi fallo — portal");
        gInPortalMode = true;
        UIManager::setStatus("WiFi fallo - portal");
        UIManager::pumpLvgl(200);
        Portal::start();
        startUITask();
        return;
    }

    if (WiFiMgr::isConnected()) {
        UIManager::setStatus("Sincronizando hora...");
        UIManager::pumpLvgl(100);
        configTime((long)gConfig.timezone_offset * 3600, 0, "pool.ntp.org", "time.nist.gov");

        // Server ANTES que servicios — OTA siempre disponible (regla #7)
        UIManager::setStatus("Servidor web...");
        UIManager::pumpLvgl(100);
        Web::begin(gConfig);

        UIManager::setStatus("Servicios...");
        UIManager::pumpLvgl(100);
        Dash::begin(gConfig);   // un solo cliente del agente (claude+pc+tokens+pomodoro+tareas)
        WiFiMgr::startAutoReconnect(gConfig);

        UIManager::setStatus("Listo!");
        UIManager::pumpLvgl(300);
    }

    startUITask();
}

void loop() {
    if (gInPortalMode) {
        Portal::handle();   // reinicia tras guardar config
        return;
    }
    if (Ota::shouldReboot() || Web::shouldReboot()) {
        delay(500);
        ESP.restart();
    }
    delay(50);
}
