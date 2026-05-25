#include "ota.h"
#include <Update.h>
#include <Arduino.h>

static bool    s_shouldReboot = false;
static Config* s_cfg          = nullptr;
static bool    s_otaDenied    = false;   // upload sin auth: ignorar chunks

// true si OTA esta protegido y la request NO trae credenciales validas
static bool otaUnauthorized(AsyncWebServerRequest* req) {
    if (!s_cfg || s_cfg->ota_pass[0] == '\0') return false;   // sin clave: abierto
    return !req->authenticate("admin", s_cfg->ota_pass);
}

static const char UPDATE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA — Nerd Dashboard</title>
<style>
  body{background:#0b0f17;color:#e6edf3;font-family:system-ui,sans-serif;
       display:flex;min-height:100vh;align-items:center;justify-content:center;margin:0;padding:16px}
  .c{background:#131a26;border:1px solid #243044;border-radius:14px;padding:26px;max-width:420px;width:100%}
  h1{color:#7af;margin:0 0 4px;font-size:20px}
  p{color:#8b97a8;font-size:13px}
  input[type=file]{width:100%;margin:14px 0;color:#e6edf3}
  button{width:100%;background:#7af;color:#001;border:0;border-radius:8px;padding:12px;font-weight:700;cursor:pointer}
  #bar{height:8px;background:#243044;border-radius:4px;overflow:hidden;margin-top:14px;display:none}
  #fill{height:100%;width:0;background:#5d8;transition:width .2s}
  #st{text-align:center;margin-top:10px;font-size:13px}
</style></head><body><div class="c">
  <h1>Actualizacion OTA</h1>
  <p>Subir firmware compilado (<code>firmware.bin</code>).</p>
  <form id="f" method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="update" accept=".bin" required>
    <button type="submit">Flashear</button>
  </form>
  <div id="bar"><div id="fill"></div></div>
  <div id="st"></div>
<script>
  const f=document.getElementById('f');
  f.onsubmit=function(e){
    e.preventDefault();
    const fd=new FormData(f), xhr=new XMLHttpRequest();
    document.getElementById('bar').style.display='block';
    xhr.upload.onprogress=function(ev){
      if(ev.lengthComputable){
        document.getElementById('fill').style.width=(ev.loaded/ev.total*100)+'%';
      }
    };
    xhr.onload=function(){
      document.getElementById('st').textContent=xhr.responseText;
    };
    xhr.open('POST','/update'); xhr.send(fd);
  };
</script></div></body></html>
)rawliteral";

void Ota::registerRoutes(AsyncWebServer& srv, Config& cfg) {
    s_cfg = &cfg;

    srv.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (otaUnauthorized(req)) { req->requestAuthentication(); return; }
        req->send(200, "text/html", UPDATE_HTML);
    });

    srv.on("/update", HTTP_POST,
        // respuesta final (tras recibir todo el archivo)
        [](AsyncWebServerRequest* req) {
            if (otaUnauthorized(req)) { req->requestAuthentication(); return; }
            bool ok = !Update.hasError();
            AsyncWebServerResponse* res = req->beginResponse(
                200, "text/plain", ok ? "OK — reiniciando..." : "FALLO en OTA");
            res->addHeader("Connection", "close");
            req->send(res);
            s_shouldReboot = ok;
        },
        // handler de upload chunked
        [](AsyncWebServerRequest* req, String filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                // Gate de auth ANTES de escribir flash. Las cabeceras (incl.
                // Authorization) ya estan parseadas en el primer chunk.
                s_otaDenied = otaUnauthorized(req);
                if (s_otaDenied) {
                    Serial.println("[ota] rechazado: sin auth");
                    return;
                }
                Serial.printf("[ota] inicio: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Update.printError(Serial);
                }
            }
            if (s_otaDenied) return;   // no escribir nada de una subida no autorizada
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[ota] OK — %u bytes\n", (unsigned)(index + len));
                } else {
                    Update.printError(Serial);
                }
            }
        }
    );
}

bool Ota::shouldReboot() { return s_shouldReboot; }
