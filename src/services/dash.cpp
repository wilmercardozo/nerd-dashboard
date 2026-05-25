#include "dash.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static ServiceItem   s_services[DASH_MAX_SERVICES];
static int           s_svcN = 0;
static ClaudeUsage   s_usage;
static PcMetrics      s_pc;
static PomoState     s_pomo;
static Task          s_tasks[8];
static int           s_taskN = 0;
static uint32_t      s_pomoPollMs = 0;
static volatile bool s_dirty = false;
static portMUX_TYPE  s_mux = portMUX_INITIALIZER_UNLOCKED;
static Config*       s_cfg = nullptr;

static PomoMode modeFromStr(const char* m) {
    if (!strcmp(m, "work")) return POMO_WORK;
    if (!strcmp(m, "short_break")) return POMO_SHORT;
    if (!strcmp(m, "long_break")) return POMO_LONG;
    return POMO_IDLE;
}

static int s_fail = 0;

// Offline duro e inmediato (sin agente configurado / sin WiFi): es offline real.
static void markHardOffline() {
    portENTER_CRITICAL(&s_mux);
    s_pc.online = false;
    s_usage.rate_ok = false;
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
}

// Fallo de poll (agente/WiFi transitorio): tolerar 3 fallos antes de marcar
// offline. Conserva el ultimo dato mientras tanto (evita el flicker).
static void onFail() {
    portENTER_CRITICAL(&s_mux);
    if (++s_fail >= 3) {
        s_pc.online = false;
        s_usage.rate_ok = false;
    }
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
}

static void fetchDashboard() {
    if (!s_cfg || s_cfg->pc_agent_host[0] == '\0' || WiFi.status() != WL_CONNECTED) {
        markHardOffline();
        return;
    }
    char url[96];
    snprintf(url, sizeof(url), "http://%s:%u/dashboard", s_cfg->pc_agent_host, s_cfg->pc_agent_port);
    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, url)) { onFail(); return; }
    http.setTimeout(4000);
    if (s_cfg->api_token[0]) http.addHeader("X-Token", s_cfg->api_token);
    int code = http.GET();
    if (code != 200) { http.end(); onFail(); return; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) { onFail(); return; }

    // ── servicios (monitores) ──
    ServiceItem sv[DASH_MAX_SERVICES];
    int sn = 0;
    for (JsonObject x : doc["services"].as<JsonArray>()) {
        if (sn >= DASH_MAX_SERVICES) break;
        strlcpy(sv[sn].name, x["name"] | "", sizeof(sv[sn].name));
        strlcpy(sv[sn].status, x["status"] | "unknown", sizeof(sv[sn].status));
        strlcpy(sv[sn].detail, x["detail"] | "", sizeof(sv[sn].detail));
        sn++;
    }

    // ── rate (tokens) ──
    ClaudeUsage u;
    JsonObject r = doc["rate"];
    u.rate_ok = r["ok"] | false;
    if (u.rate_ok) {
        u.rate_5h_pct = r["s"] | 0;
        u.rate_5h_reset_min = r["sr"] | 0;
        u.rate_7d_pct = r["w"] | 0;
        u.rate_7d_reset_min = r["wr"] | 0;
        strlcpy(u.rate_status, r["st"] | "unknown", sizeof(u.rate_status));
    }

    // ── pc ──
    PcMetrics m;
    JsonObject pc = doc["pc"];
    if (pc["cpu"].is<int>()) {
        m.online = true;
        strlcpy(m.host, pc["host"] | "", sizeof(m.host));
        strlcpy(m.os, pc["os"] | "", sizeof(m.os));
        m.cpu = pc["cpu"] | 0;
        m.ram = pc["ram"] | 0;
        m.disk = pc["disk"] | -1;
        m.gpu = pc["gpu"] | -1;
        m.gpu_mem = pc["gpu_mem"] | -1;
        m.cpu_temp = pc["cpu_temp"] | -1;
        m.gpu_temp = pc["gpu_temp"] | -1;
        m.batt = pc["batt"] | -1;
        m.batt_plugged = pc["batt_plugged"] | false;
        m.net_up = pc["net_up"] | 0;
        m.net_down = pc["net_down"] | 0;
        m.cpu_count = pc["cpu_count"] | -1;
        m.cpu_freq = pc["cpu_freq"] | -1;
        m.mem_total = pc["mem_total"] | 0.0f;
        m.mem_used = pc["mem_used"] | 0.0f;
        m.pc_uptime_s = pc["uptime_s"] | 0;
        strlcpy(m.gpu_name, pc["gpu_name"] | "", sizeof(m.gpu_name));
    }

    // ── pomodoro ──
    PomoState p;
    JsonObject po = doc["pomodoro"];
    p.mode = modeFromStr(po["mode"] | "idle");
    p.duration_s = po["duration_s"] | 0;
    p.remaining_s = po["remaining_s"] | 0;
    p.paused = po["paused"] | false;
    strlcpy(p.current_task_id, po["current_task_id"] | "", sizeof(p.current_task_id));
    strlcpy(p.current_task_title, po["current_task_title"] | "", sizeof(p.current_task_title));
    p.completed_today = po["completed_today"] | 0;
    JsonObject pcfg = po["config"];
    p.cfg.work_s    = pcfg["work_s"]    | 1500;
    p.cfg.short_s   = pcfg["short_s"]   | 300;
    p.cfg.long_s    = pcfg["long_s"]    | 900;
    p.cfg.long_every = pcfg["long_every"] | 4;

    // ── tareas ──
    Task tk[8];
    int tn = 0;
    for (JsonObject t : doc["tasks"].as<JsonArray>()) {
        if (tn >= 8) break;
        strlcpy(tk[tn].id, t["id"] | "", sizeof(tk[tn].id));
        strlcpy(tk[tn].title, t["title"] | "", sizeof(tk[tn].title));
        tk[tn].priority = t["priority"] | 1;
        tk[tn].done = t["done"] | false;
        tn++;
    }

    portENTER_CRITICAL(&s_mux);
    s_fail = 0;            // poll OK: resetear tolerancia
    for (int i = 0; i < sn; i++) s_services[i] = sv[i];
    s_svcN = sn;
    s_usage = u;
    s_pc = m;
    s_pomo = p;
    s_pomoPollMs = millis();
    for (int i = 0; i < tn; i++) s_tasks[i] = tk[i];
    s_taskN = tn;
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
}

static void req(const char* method, const char* path, const char* body) {
    if (!s_cfg || s_cfg->pc_agent_host[0] == '\0') return;
    char url[96];
    snprintf(url, sizeof(url), "http://%s:%u%s", s_cfg->pc_agent_host, s_cfg->pc_agent_port, path);
    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, url)) return;
    http.setTimeout(3000);
    if (s_cfg->api_token[0]) http.addHeader("X-Token", s_cfg->api_token);
    if (body && body[0]) http.addHeader("Content-Type", "application/json");
    http.sendRequest(method, (uint8_t*)(body ? body : ""), body ? strlen(body) : 0);
    http.end();
}

// ── cola de comandos: el toque encola (no bloquea), el worker hace el POST ──
enum CmdType { C_START, C_PAUSE, C_RESET, C_SELECT, C_TASKDONE };
struct Cmd { uint8_t type; char mode[12]; char task[TASK_ID_MAX]; int8_t flag; };
static QueueHandle_t s_q = nullptr;

static void doCmd(const Cmd& c) {
    char b[96];
    switch (c.type) {
        case C_START:
            snprintf(b, sizeof(b), "{\"mode\":\"%s\",\"task_id\":\"%s\"}", c.mode, c.task);
            req("POST", "/pomodoro/start", b);
            break;
        case C_PAUSE: req("POST", "/pomodoro/pause", ""); break;
        case C_RESET: req("POST", "/pomodoro/reset", ""); break;
        case C_SELECT:
            snprintf(b, sizeof(b), "{\"task_id\":\"%s\"}", c.task);
            req("POST", "/pomodoro/select", b);
            break;
        case C_TASKDONE:
            snprintf(b, sizeof(b), "{\"id\":\"%s\",\"done\":%s}", c.task, c.flag ? "true" : "false");
            req("PUT", "/tasks", b);
            break;
    }
}

// Tarea unica: espera comandos hasta 2s; si llega uno lo manda + refetch
// inmediato; si no, hace el poll periodico. UI nunca se bloquea.
static void taskDash(void*) {
    for (;;) {
        Cmd c;
        if (s_q && xQueueReceive(s_q, &c, pdMS_TO_TICKS(2000)) == pdTRUE) {
            doCmd(c);
            fetchDashboard();   // reflejar el cambio de inmediato
        } else {
            fetchDashboard();
        }
    }
}

namespace Dash {

void begin(Config& cfg) {
    s_cfg = &cfg;
    s_q = xQueueCreate(8, sizeof(Cmd));
    xTaskCreatePinnedToCore(taskDash, "dash", 8192, nullptr, 1, nullptr, 0);
}

bool consume() {
    bool d;
    portENTER_CRITICAL(&s_mux);
    d = s_dirty; s_dirty = false;
    portEXIT_CRITICAL(&s_mux);
    return d;
}

int services(ServiceItem* out, int maxN) {
    portENTER_CRITICAL(&s_mux);
    int n = s_svcN < maxN ? s_svcN : maxN;
    for (int i = 0; i < n; i++) out[i] = s_services[i];
    portEXIT_CRITICAL(&s_mux);
    return n;
}
ClaudeUsage  usage()  { ClaudeUsage o;  portENTER_CRITICAL(&s_mux); o = s_usage;  portEXIT_CRITICAL(&s_mux); return o; }
PcMetrics    pc()     { PcMetrics o;    portENTER_CRITICAL(&s_mux); o = s_pc;     portEXIT_CRITICAL(&s_mux); return o; }

PomoState pomo() {
    PomoState p; uint32_t pollMs;
    portENTER_CRITICAL(&s_mux);
    p = s_pomo; pollMs = s_pomoPollMs;
    portEXIT_CRITICAL(&s_mux);
    // interpolar el countdown entre polls (timer suave)
    if (p.mode != POMO_IDLE && !p.paused) {
        uint32_t elapsed = (millis() - pollMs) / 1000;
        p.remaining_s = (p.remaining_s > elapsed) ? (p.remaining_s - elapsed) : 0;
    }
    return p;
}

int tasks(Task* out, int maxN) {
    portENTER_CRITICAL(&s_mux);
    int n = s_taskN < maxN ? s_taskN : maxN;
    for (int i = 0; i < n; i++) out[i] = s_tasks[i];
    portEXIT_CRITICAL(&s_mux);
    return n;
}

static void enqueue(const Cmd& c) {
    if (s_q) xQueueSend(s_q, &c, 0);
}

void pomoStart(const char* mode, const char* taskId) {
    if (!mode) mode = "work";
    // optimista: el display arranca YA (el server confirma en el refetch)
    portENTER_CRITICAL(&s_mux);
    s_pomo.mode = modeFromStr(mode);
    s_pomo.duration_s = (s_pomo.mode == POMO_WORK)  ? s_pomo.cfg.work_s
                      : (s_pomo.mode == POMO_SHORT) ? s_pomo.cfg.short_s
                      : (s_pomo.mode == POMO_LONG)  ? s_pomo.cfg.long_s : 0;
    s_pomo.remaining_s = s_pomo.duration_s;
    s_pomo.paused = false;
    if (taskId && taskId[0]) strlcpy(s_pomo.current_task_id, taskId, sizeof(s_pomo.current_task_id));
    s_pomoPollMs = millis();
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
    Cmd c = {C_START};
    strlcpy(c.mode, mode, sizeof(c.mode));
    strlcpy(c.task, taskId ? taskId : "", sizeof(c.task));
    enqueue(c);
}

void pomoPause() {
    portENTER_CRITICAL(&s_mux);
    if (s_pomo.mode != POMO_IDLE) {
        if (!s_pomo.paused) {
            uint32_t el = (millis() - s_pomoPollMs) / 1000;
            s_pomo.remaining_s = s_pomo.remaining_s > el ? s_pomo.remaining_s - el : 0;
            s_pomo.paused = true;
        } else {
            s_pomo.paused = false;
            s_pomoPollMs = millis();
        }
        s_dirty = true;
    }
    portEXIT_CRITICAL(&s_mux);
    Cmd c = {C_PAUSE};
    enqueue(c);
}

void pomoReset() {
    portENTER_CRITICAL(&s_mux);
    s_pomo.mode = POMO_IDLE;
    s_pomo.remaining_s = 0;
    s_pomo.paused = false;
    s_pomo.current_task_id[0] = '\0';
    s_pomo.current_task_title[0] = '\0';
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
    Cmd c = {C_RESET};
    enqueue(c);
}

void pomoSelect(const char* taskId) {
    portENTER_CRITICAL(&s_mux);
    strlcpy(s_pomo.current_task_id, taskId ? taskId : "", sizeof(s_pomo.current_task_id));
    s_pomo.current_task_title[0] = '\0';
    for (int i = 0; i < s_taskN; i++) {
        if (strcmp(s_tasks[i].id, s_pomo.current_task_id) == 0) {
            strlcpy(s_pomo.current_task_title, s_tasks[i].title, sizeof(s_pomo.current_task_title));
            break;
        }
    }
    s_dirty = true;
    portEXIT_CRITICAL(&s_mux);
    Cmd c = {C_SELECT};
    strlcpy(c.task, taskId ? taskId : "", sizeof(c.task));
    enqueue(c);
}

void taskDone(const char* id, bool done) {
    Cmd c = {C_TASKDONE};
    strlcpy(c.task, id ? id : "", sizeof(c.task));
    c.flag = done ? 1 : 0;
    enqueue(c);
}

} // namespace Dash
