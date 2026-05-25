#include "WiFiManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static bool s_needsPortal = false;
static bool s_reconnectStarted = false;

bool WiFiMgr::connect(const Config& cfg) {
    if (cfg.wifi_ssid[0] == '\0') {
        s_needsPortal = true;
        return false;
    }

    Serial.printf("[wifi] Connecting to %s ...\n", cfg.wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

    uint32_t deadline = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] Connected — IP %s\n", WiFi.localIP().toString().c_str());

        // Algunos routers entregan su propia IP como DNS pero no resuelven nada
        // (visto en un router MAKKI: hostByName "DNS Failed"). Forzamos DNS
        // publico conservando IP/gateway/mascara del DHCP.
        IPAddress ip   = WiFi.localIP();
        IPAddress gw   = WiFi.gatewayIP();
        IPAddress mask = WiFi.subnetMask();
        WiFi.config(ip, gw, mask, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
        Serial.println("[wifi] DNS set to 1.1.1.1 / 8.8.8.8");
        return true;
    }

    s_needsPortal = true;
    Serial.println("[wifi] Connection failed — portal required");
    return false;
}

bool WiFiMgr::isConnected() { return WiFi.status() == WL_CONNECTED; }
bool WiFiMgr::needsPortal() { return s_needsPortal; }
void WiFiMgr::clearPortalFlag() { s_needsPortal = false; }

static void taskWiFiReconnect(void* param) {
    const Config* cfg = (const Config*)param;
    uint32_t backoffMs        = 10000;
    const uint32_t maxBackoff = 120000;
    uint32_t nextAttemptAt    = 0;
    bool wasConnected         = (WiFi.status() == WL_CONNECTED);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        bool connectedNow = (WiFi.status() == WL_CONNECTED);

        if (connectedNow) {
            if (!wasConnected) {
                Serial.printf("[wifi] Reconnected — IP %s\n", WiFi.localIP().toString().c_str());
                backoffMs = 10000;
                nextAttemptAt = 0;
            }
            wasConnected = true;
            continue;
        }

        wasConnected = false;
        uint32_t now = millis();
        if (now < nextAttemptAt) continue;
        if (cfg->wifi_ssid[0] == '\0') continue;

        Serial.printf("[wifi] Link down — reconnect (next backoff %lus)\n",
                      (unsigned long)(backoffMs / 1000));
        // Full begin() en vez de reconnect(): el reconnect() del ESP32 a veces
        // queda atascado en estado "no-scan" tras desaparecer el AP.
        WiFi.disconnect(false, false);
        vTaskDelay(pdMS_TO_TICKS(200));
        WiFi.begin(cfg->wifi_ssid, cfg->wifi_pass);

        nextAttemptAt = millis() + backoffMs;
        backoffMs = (backoffMs * 2 > maxBackoff) ? maxBackoff : backoffMs * 2;
    }
}

void WiFiMgr::startAutoReconnect(const Config& cfg) {
    if (s_reconnectStarted) return;
    s_reconnectStarted = true;
    xTaskCreatePinnedToCore(taskWiFiReconnect, "wifi-reconn",
                            4096, (void*)&cfg, 1, nullptr, 0);
}
