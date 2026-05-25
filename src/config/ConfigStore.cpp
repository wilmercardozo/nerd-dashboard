#include "ConfigStore.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char* PATH              = "/config.json";
static const char* PATH_FORCE_PORTAL = "/force_portal";

bool ConfigStore::exists() {
    return LittleFS.exists(PATH);
}

void ConfigStore::erase() {
    LittleFS.remove(PATH);
}

bool ConfigStore::load(Config& out) {
    if (!LittleFS.exists(PATH)) return false;
    File f = LittleFS.open(PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err != DeserializationError::Ok) return false;

    strlcpy(out.wifi_ssid,    doc["wifi_ssid"]    | "",              sizeof(out.wifi_ssid));
    strlcpy(out.wifi_pass,    doc["wifi_pass"]    | "",              sizeof(out.wifi_pass));
    strlcpy(out.device_name,  doc["device_name"]  | "nerddashboard", sizeof(out.device_name));
    out.timezone_offset = doc["timezone_offset"] | -5;

    strlcpy(out.admin_key,    doc["admin_key"]    | "", sizeof(out.admin_key));
    out.budget_monthly_usd = doc["budget_monthly_usd"] | 30.0f;
    out.web_msg_limit      = doc["web_msg_limit"]      | 45;

    strlcpy(out.pc_agent_host, doc["pc_agent_host"] | "", sizeof(out.pc_agent_host));
    out.pc_agent_port = doc["pc_agent_port"] | 8765;
    out.pc_poll_ms    = doc["pc_poll_ms"]    | 2000;

    strlcpy(out.api_token, doc["api_token"] | "", sizeof(out.api_token));
    strlcpy(out.ota_pass,  doc["ota_pass"]  | "", sizeof(out.ota_pass));

    out.valid = true;
    return true;
}

bool ConfigStore::isForcePortal() {
    if (!LittleFS.exists(PATH_FORCE_PORTAL)) return false;
    LittleFS.remove(PATH_FORCE_PORTAL);   // one-shot: limpiar tras leer
    return true;
}

void ConfigStore::setForcePortal() {
    File f = LittleFS.open(PATH_FORCE_PORTAL, "w");
    if (f) { f.print("1"); f.close(); }
}

bool ConfigStore::save(const Config& cfg) {
    JsonDocument doc;
    doc["wifi_ssid"]          = cfg.wifi_ssid;
    doc["wifi_pass"]          = cfg.wifi_pass;
    doc["device_name"]        = cfg.device_name;
    doc["timezone_offset"]    = cfg.timezone_offset;
    doc["admin_key"]          = cfg.admin_key;
    doc["budget_monthly_usd"] = cfg.budget_monthly_usd;
    doc["web_msg_limit"]      = cfg.web_msg_limit;
    doc["pc_agent_host"]      = cfg.pc_agent_host;
    doc["pc_agent_port"]      = cfg.pc_agent_port;
    doc["pc_poll_ms"]         = cfg.pc_poll_ms;
    doc["api_token"]          = cfg.api_token;
    doc["ota_pass"]           = cfg.ota_pass;

    File f = LittleFS.open(PATH, "w");
    if (!f) return false;
    size_t written = serializeJson(doc, f);
    f.close();
    return written > 0;
}
