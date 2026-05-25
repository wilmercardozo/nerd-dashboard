#include "portal.h"
#include "config.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config/ConfigStore.h"
#include "device_config.h"

static AsyncWebServer s_server(80);
static bool s_done = false;

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nerd Dashboard — Setup</title>
<style>
  :root { --bg:#0b0f17; --card:#131a26; --bd:#243044; --fg:#e6edf3; --mut:#8b97a8; --cy:#7af; --gr:#5d8; }
  * { box-sizing:border-box; }
  body { margin:0; min-height:100vh; background:var(--bg); color:var(--fg);
         font-family:system-ui,-apple-system,sans-serif; display:flex; align-items:center;
         justify-content:center; padding:16px; }
  .card { background:var(--card); border:1px solid var(--bd); border-radius:14px;
          padding:26px; width:100%; max-width:420px; box-shadow:0 12px 40px rgba(0,0,0,.5); }
  h1 { margin:0 0 2px; font-size:22px; color:var(--cy); }
  .sub { color:var(--mut); font-size:13px; margin:0 0 20px; }
  label { display:block; font-size:11px; text-transform:uppercase; letter-spacing:1px;
          color:var(--mut); margin:14px 0 6px; }
  input,select { width:100%; background:var(--bg); border:1px solid var(--bd); color:var(--fg);
                 border-radius:8px; padding:10px; font-size:14px; }
  .row { display:flex; gap:8px; }
  .row input { flex:1; }
  button { cursor:pointer; }
  .scan { background:transparent; border:1px solid var(--bd); color:var(--mut);
          border-radius:8px; padding:0 14px; font-size:12px; }
  .nets { margin-top:8px; max-height:170px; overflow-y:auto; border:1px solid var(--bd);
          border-radius:8px; display:none; }
  .net { display:flex; justify-content:space-between; padding:9px 11px; cursor:pointer;
         border-bottom:1px solid var(--bd); font-size:13px; }
  .net:last-child { border-bottom:0; }
  .net:hover { background:var(--bd); }
  .bars { font-family:monospace; color:var(--gr); }
  .submit { width:100%; margin-top:22px; background:var(--cy); color:#001; border:0;
            border-radius:8px; padding:13px; font-weight:700; font-size:15px; }
  .hint { padding:9px; text-align:center; color:var(--mut); font-size:12px; }
  #msg { text-align:center; margin-top:14px; font-size:14px; display:none; }
</style>
</head>
<body>
<div class="card">
  <h1>Nerd Dashboard</h1>
  <p class="sub">Configuracion inicial</p>
  <form id="f">
    <label>Red WiFi</label>
    <div class="row">
      <input id="ssid" name="wifi_ssid" placeholder="SSID" required>
      <button type="button" class="scan" id="sb" onclick="scan()">Buscar</button>
    </div>
    <div class="nets" id="nets"></div>
    <label>Contrasena</label>
    <input name="wifi_pass" type="password" placeholder="(vacio si es abierta)">
    <label>Nombre del dispositivo</label>
    <input name="device_name" value="nerddashboard">
    <label>IP del PC (agente) — donde corre pc-agent</label>
    <input name="pc_agent_host" placeholder="192.168.0.107 (opcional)">
    <label>Zona horaria</label>
    <select name="timezone_offset">
      <option value="-5" selected>UTC-5 (Colombia/Peru/Ecuador)</option>
      <option value="-4">UTC-4 (Venezuela/Bolivia)</option>
      <option value="-3">UTC-3 (Argentina/Brasil)</option>
      <option value="-6">UTC-6 (Mexico)</option>
      <option value="1">UTC+1 (Espana)</option>
      <option value="0">UTC+0</option>
    </select>
    <button class="submit" type="submit">Guardar y conectar</button>
  </form>
  <p id="msg"></p>
</div>
<script>
  function bars(r){
    const n = r>=-50?4:r>=-60?3:r>=-70?2:r>=-80?1:0;
    return '<span class="bars">'+'█'.repeat(n)+'░'.repeat(4-n)+'</span>';
  }
  function scan(){
    const L=document.getElementById('nets'), B=document.getElementById('sb');
    B.textContent='...'; B.disabled=true; L.style.display='block';
    L.innerHTML='<div class="hint">Buscando redes...</div>';
    fetch('/scan').then(r=>r.json()).then(ns=>{
      B.textContent='Buscar'; B.disabled=false;
      if(!ns.length){L.innerHTML='<div class="hint">Sin redes</div>';return;}
      ns.sort((a,b)=>b.rssi-a.rssi);
      L.innerHTML=ns.map(n=>'<div class="net" onclick="pick(\''+n.ssid.replace(/'/g,"\\'")+'\')"><span>'+(n.secure?'\u{1F512} ':'')+n.ssid+'</span>'+bars(n.rssi)+'</div>').join('');
    }).catch(()=>{B.textContent='Buscar';B.disabled=false;L.innerHTML='<div class="hint">Error al escanear</div>';});
  }
  function pick(s){document.getElementById('ssid').value=s;document.getElementById('nets').style.display='none';}
  document.getElementById('f').onsubmit=function(e){
    e.preventDefault();
    const d=Object.fromEntries(new FormData(this));
    fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})
    .then(r=>r.json()).then(d=>{
      const m=document.getElementById('msg');
      m.style.display='block';
      m.style.color=d.ok?'#5d8':'#f77';
      m.textContent=d.ok?'Guardado — reiniciando...':'Error: '+d.error;
    });
  };
</script>
</html>
)rawliteral";

void Portal::start() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    Serial.printf("[portal] AP \"%s\" — abrir http://192.168.4.1\n", AP_SSID);

    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", PORTAL_HTML);
    });

    s_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        int n = WiFi.scanNetworks();
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n && i < 15; i++) {
            if (WiFi.SSID(i).length() == 0) continue;
            JsonObject obj = arr.add<JsonObject>();
            obj["ssid"]   = WiFi.SSID(i);
            obj["rssi"]   = (int)WiFi.RSSI(i);
            obj["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
        WiFi.scanDelete();
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    s_server.on("/save", HTTP_POST,
        [](AsyncWebServerRequest* req) {},
        nullptr,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"JSON invalido\"}");
                return;
            }
            Config cfg;
            strlcpy(cfg.wifi_ssid,   doc["wifi_ssid"]   | "",              sizeof(cfg.wifi_ssid));
            strlcpy(cfg.wifi_pass,   doc["wifi_pass"]   | "",              sizeof(cfg.wifi_pass));
            strlcpy(cfg.device_name, doc["device_name"] | "nerddashboard", sizeof(cfg.device_name));
            strlcpy(cfg.pc_agent_host, doc["pc_agent_host"] | "", sizeof(cfg.pc_agent_host));
            // FormData manda todo como string; .as<int>() parsea numericos string.
            int tz = doc["timezone_offset"].as<int>();
            cfg.timezone_offset = (int8_t)((tz >= -12 && tz <= 14) ? tz : -5);

            if (cfg.wifi_ssid[0] == '\0') {
                req->send(200, "application/json", "{\"ok\":false,\"error\":\"SSID requerido\"}");
                return;
            }
            cfg.valid = true;
            ConfigStore::save(cfg);
            Serial.println("[portal] Config saved — restarting");
            req->send(200, "application/json", "{\"ok\":true}");
            s_done = true;   // el reinicio ocurre en Portal::handle() desde loop()
        }
    );

    s_server.begin();
}

void Portal::handle() {
    if (s_done) {
        // No suspender el scheduler aqui. ESP.restart() es un reset de hardware.
        delay(500);   // dar tiempo a que el HTTP 200 llegue al navegador
        ESP.restart();
    }
}

void Portal::stop() { s_server.end(); }
bool Portal::isDone() { return s_done; }
