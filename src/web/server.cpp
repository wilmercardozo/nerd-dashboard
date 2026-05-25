#include "server.h"
#include "ota.h"
#include "config.h"
#include "device_config.h"
#include "config/ConfigStore.h"

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Servidor MINIMO del ESP: el dashboard pesado (tareas/pomodoro/claude/pc) vive
// en el agente PC. El ESP solo expone: OTA, una landing para fijar la IP del
// agente, y /api/status. Asi el AsyncTCP del ESP no se satura (no mas timeouts).
static AsyncWebServer s_server(80);
static Config*        s_cfg = nullptr;
static volatile bool  s_reboot = false;

static const char LANDING[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Nerd Dashboard</title>
<style>body{background:#0b0f17;color:#e6edf3;font-family:system-ui,sans-serif;margin:0 auto;padding:24px;max-width:480px}
h1{color:#4fd1c5}a{color:#4fd1c5}input{width:100%;box-sizing:border-box;background:#131a26;border:1px solid #243044;color:#e6edf3;border-radius:8px;padding:10px;margin-top:6px}
button{width:100%;margin-top:12px;background:#4fd1c5;color:#012;border:0;border-radius:8px;padding:12px;font-weight:700;cursor:pointer}.mut{color:#8b97a8;font-size:13px}hr{border:0;border-top:1px solid #243044;margin:18px 0}</style>
</head><body>
<h1>Nerd Dashboard</h1>
<p class="mut">El dashboard completo (tareas, pomodoro, Claude, PC) corre en el <b>agente PC</b>.</p>
<p style="font-size:18px"><a id="link" href="#">Abrir dashboard del agente &rarr;</a></p>
<hr>
<h3>Agente PC</h3>
<p class="mut">IP de la PC donde corre <code>pc-agent</code> (puerto 8765).</p>
<input id="host" placeholder="192.168.0.107"><input id="port" type="number" value="8765">
<label>Token (solo si el agente lo exige)</label><input id="token" type="password" placeholder="(opcional)">
<button onclick="save()">Guardar</button>
<p id="msg" class="mut"></p>
<hr>
<h3>WiFi</h3>
<p class="mut">Reabre el portal de configuración (red <code>NerdDashboard-Setup</code>) para cambiar de red.</p>
<button onclick="wifiReset()" style="background:#243044;color:#ff5d6c">Reconfigurar WiFi</button>
<p id="wmsg" class="mut"></p>
<hr><p><a href="/update">Actualizar firmware (OTA) &rarr;</a></p>
<script>
const $=i=>document.getElementById(i);
function upd(){$('link').href='http://'+$('host').value+':'+$('port').value+'/';}
async function load(){try{const s=await(await fetch('/api/status')).json();$('host').value=s.pc_agent_host||'';$('port').value=s.pc_agent_port||8765;upd();}catch(e){}}
async function save(){const b={agent_host:$('host').value.trim(),agent_port:+$('port').value||8765};
 if($('token').value.trim())b.api_token=$('token').value.trim();
 try{const r=await(await fetch('/api/config/pc',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)})).json();
 $('msg').textContent=r.ok?('Guardado: '+r.agent_host+':'+r.agent_port):'Error';upd();}catch(e){$('msg').textContent='Error';}}
async function wifiReset(){if(!confirm('¿Reabrir portal de WiFi? El dispositivo reiniciará y creará la red NerdDashboard-Setup.'))return;
 try{await fetch('/api/wifi/reset',{method:'POST'});$('wmsg').textContent='Reiniciando al portal... conéctate a NerdDashboard-Setup.';}catch(e){$('wmsg').textContent='Error';}}
load();
</script></body></html>
)rawliteral";

namespace Web {

void begin(Config& cfg) {
    s_cfg = &cfg;
    if (MDNS.begin(cfg.device_name)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[web] mDNS: http://%s.local\n", cfg.device_name);
    }

    Ota::registerRoutes(s_server);

    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", LANDING);
    });

    s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["device"]   = MDNS_NAME;
        doc["model"]    = DEVICE_MODEL;
        doc["fw"]       = FW_VERSION;
        doc["ip"]       = WiFi.localIP().toString();
        doc["rssi"]     = (int)WiFi.RSSI();
        doc["uptime_s"] = (uint32_t)(millis() / 1000);
        doc["heap"]     = (uint32_t)ESP.getFreeHeap();
        if (s_cfg) {
            doc["pc_agent_host"] = s_cfg->pc_agent_host;
            doc["pc_agent_port"] = s_cfg->pc_agent_port;
        }
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    s_server.on("/api/config/pc", HTTP_POST,
        [](AsyncWebServerRequest*) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok || !s_cfg) {
                req->send(200, "application/json", "{\"ok\":false}");
                return;
            }
            if (doc["agent_host"].is<const char*>())
                strlcpy(s_cfg->pc_agent_host, doc["agent_host"] | "", sizeof(s_cfg->pc_agent_host));
            if (doc["agent_port"].is<int>())
                s_cfg->pc_agent_port = (uint16_t)doc["agent_port"].as<int>();
            if (doc["api_token"].is<const char*>())
                strlcpy(s_cfg->api_token, doc["api_token"] | "", sizeof(s_cfg->api_token));
            ConfigStore::save(*s_cfg);
            Serial.printf("[web] agente PC: %s:%u\n", s_cfg->pc_agent_host, s_cfg->pc_agent_port);
            JsonDocument r;
            r["ok"] = true;
            r["agent_host"] = s_cfg->pc_agent_host;
            r["agent_port"] = s_cfg->pc_agent_port;
            String out; serializeJson(r, out);
            req->send(200, "application/json", out);
        }
    );

    // Reabrir portal de WiFi: marca el centinela force-portal y reinicia.
    s_server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
        ConfigStore::setForcePortal();
        s_reboot = true;
        req->send(200, "application/json", "{\"ok\":true}");
    });

    s_server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "application/json", "{\"error\":\"not found\"}");
    });

    s_server.begin();
    Serial.println("[web] server minimo on :80 (landing + OTA + config)");
}

bool shouldReboot() { return s_reboot; }

} // namespace Web
