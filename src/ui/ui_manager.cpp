#include "ui_manager.h"
#include "config.h"
#include "view_claude.h"
#include "view_usage.h"
#include "view_pc.h"
#include "view_tasks.h"
#include "services/dash.h"

#include <TFT_eSPI.h>
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ── estado estatico ────────────────────────────────────────────────────
static TFT_eSPI       s_tft;
static lv_display_t*  s_disp = nullptr;
// 16 filas x 320 px x 2 B = 10 KB — en DRAM (CYD sin PSRAM)
static lv_color_t     s_lvBuf[SCREEN_W * 16];

// touch en bus VSPI (separado del TFT en HSPI)
static SPIClass            s_touchSPI(VSPI);
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);

static lv_obj_t* s_statusLabel = nullptr;   // barra inferior
static uint32_t  s_lastBar     = 0;
static char      s_bootMsg[40] = "";        // texto temporal de arranque

// ── flush TFT_eSPI ─────────────────────────────────────────────────────
static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    s_tft.startWrite();
    s_tft.setAddrWindow(area->x1, area->y1, w, h);
    s_tft.pushColors(reinterpret_cast<uint16_t*>(px_map), w * h, true);
    s_tft.endWrite();
    lv_display_flush_ready(disp);
}

// ── lectura touch para LVGL (navegacion nativa del tabview) ─────────────
static void touchReadCb(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    if (s_touch.tirqTouched() && s_touch.touched()) {
        TS_Point p = s_touch.getPoint();
        // rotacion 3: invertir ambos ejes desde coords crudas XPT2046
        int16_t x = (int16_t)map(p.x, TOUCH_RAW_MIN, TOUCH_RAW_MAX, SCREEN_W - 1, 0);
        int16_t y = (int16_t)map(p.y, TOUCH_RAW_MIN, TOUCH_RAW_MAX, SCREEN_H - 1, 0);
        data->point.x = constrain(x, 0, SCREEN_W - 1);
        data->point.y = constrain(y, 0, SCREEN_H - 1);
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ── construccion de la UI ──────────────────────────────────────────────
static void buildUI() {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0f17), 0);

    const int barH = 16;

    // Tabview ocupa todo menos la barra de estado inferior
    lv_obj_t* tv = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tv, 28);
    lv_obj_set_size(tv, SCREEN_W, SCREEN_H - barH);
    lv_obj_align(tv, LV_ALIGN_TOP_LEFT, 0, 0);

    // estilo del tabview acorde al tema
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x0a0e14), 0);
    lv_obj_t* tbar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(tbar, lv_color_hex(0x0d1320), 0);
    lv_obj_set_style_text_color(tbar, lv_color_hex(0x8b97a8), 0);
    lv_obj_set_style_text_font(tbar, &lv_font_montserrat_14, 0);

    lv_obj_t* tabClaude = lv_tabview_add_tab(tv, "Servicios");
    lv_obj_t* tabTokens = lv_tabview_add_tab(tv, "Tokens");
    lv_obj_t* tabPC     = lv_tabview_add_tab(tv, "PC");
    lv_obj_t* tabTasks  = lv_tabview_add_tab(tv, "Tareas");

    ViewClaude::create(tabClaude);
    ViewUsage::create(tabTokens);
    ViewPC::create(tabPC);
    ViewTasks::create(tabTasks);

    // Barra de estado inferior (IP / uptime / heap)
    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, SCREEN_W, barH);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x131a26), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_statusLabel = lv_label_create(bar);
    lv_obj_set_style_text_font(s_statusLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_statusLabel, lv_color_hex(0x8b97a8), 0);
    lv_obj_align(s_statusLabel, LV_ALIGN_LEFT_MID, 6, 0);
    lv_label_set_text(s_statusLabel, "Iniciando...");
}

// ── barra de estado ────────────────────────────────────────────────────
static void updateStatusBar() {
    if (!s_statusLabel) return;
    char buf[96];
    uint32_t up = millis() / 1000;
    uint32_t h = up / 3600, m = (up % 3600) / 60;
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s  " LV_SYMBOL_REFRESH " %luh%02lum  RAM %uKB",
                 WiFi.localIP().toString().c_str(),
                 (unsigned long)h, (unsigned long)m,
                 (unsigned)(ESP.getFreeHeap() / 1024));
    } else if (s_bootMsg[0]) {
        snprintf(buf, sizeof(buf), "%s", s_bootMsg);
    } else {
        snprintf(buf, sizeof(buf), "Sin conexion  RAM %uKB",
                 (unsigned)(ESP.getFreeHeap() / 1024));
    }
    lv_label_set_text(s_statusLabel, buf);
}

namespace UIManager {

void init() {
    s_tft.init();
    s_tft.setRotation(3);   // landscape, USB a la izquierda

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    s_disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(s_disp, flushCb);
    lv_display_set_buffers(s_disp, s_lvBuf, nullptr, sizeof(s_lvBuf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Tema oscuro con acento teal (recolorea tabview/botones/barras/arc)
    lv_theme_t* th = lv_theme_default_init(s_disp,
                                           lv_color_hex(0x4fd1c5),   // primario teal
                                           lv_color_hex(0xf5c542),   // secundario ambar
                                           true,                     // modo oscuro
                                           &lv_font_montserrat_14);
    lv_display_set_theme(s_disp, th);

    s_touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, -1);
    s_touch.begin(s_touchSPI);
    s_touch.setRotation(1);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchReadCb);

    buildUI();
}

void tick() {
    // Consumir estado Claude si hay datos nuevos
    // Un solo poll al agente trae todo; al haber datos nuevos refresca vistas.
    if (Dash::consume()) {
        ServiceItem svc[DASH_MAX_SERVICES];
        int sn = Dash::services(svc, DASH_MAX_SERVICES);
        ViewClaude::update(svc, sn);
        ViewUsage::update(Dash::usage());
        ViewPC::update(Dash::pc());
        ViewTasks::refresh();
        ViewTasks::updatePomodoro(Dash::pomo());   // refleja toques al instante
    }
    // Timer pomodoro + barra de estado ~1 Hz (timer interpolado = suave)
    uint32_t now = millis();
    if (now - s_lastBar >= 1000) {
        s_lastBar = now;
        updateStatusBar();
        ViewTasks::updatePomodoro(Dash::pomo());
    }
    lv_timer_handler();
}

void pumpLvgl(uint32_t ms) {
    // Desde core 1 en setup(), antes de que exista la tarea UI: spin de
    // lv_timer_handler para renderizar mensajes de arranque.
    uint32_t start = millis();
    while (millis() - start < ms) {
        lv_timer_handler();
        delay(16);
    }
}

void setStatus(const char* msg) {
    strlcpy(s_bootMsg, msg ? msg : "", sizeof(s_bootMsg));
    if (s_statusLabel) lv_label_set_text(s_statusLabel, s_bootMsg);
}

} // namespace UIManager
