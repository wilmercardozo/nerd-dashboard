#include "view_pc.h"
#include <stdio.h>

static lv_obj_t* s_header = nullptr;
static lv_obj_t* s_grid   = nullptr;
static lv_obj_t* s_foot   = nullptr;
static lv_obj_t* s_offline = nullptr;

// tiles: value label + bar por metrica
static lv_obj_t* s_cpuV=nullptr,  *s_cpuB=nullptr;
static lv_obj_t* s_ramV=nullptr,  *s_ramB=nullptr;
static lv_obj_t* s_gpuTile=nullptr,*s_gpuV=nullptr,*s_gpuB=nullptr,*s_gpuName=nullptr;
static lv_obj_t* s_diskV=nullptr, *s_diskB=nullptr;

static lv_color_t lvlColor(int pct) {
    if (pct >= 90) return lv_color_hex(0xff5d5d);
    if (pct >= 70) return lv_color_hex(0xf5c542);
    return lv_color_hex(0x4fd1c5);
}
static void fmtRate(uint32_t bps, char* o, size_t n) {
    double v = bps;
    if (v >= 1e6) snprintf(o, n, "%.1fM", v / 1e6);
    else if (v >= 1e3) snprintf(o, n, "%.0fK", v / 1e3);
    else snprintf(o, n, "%lu", (unsigned long)bps);
}
static void setBar(lv_obj_t* b, int pct) {
    int v = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    lv_bar_set_value(b, v, LV_ANIM_ON);
    lv_obj_set_style_bg_color(b, lvlColor(pct), LV_PART_INDICATOR);
}

// tile: nombre (arriba) + valor grande + barra
static lv_obj_t* makeTile(lv_obj_t* parent, const char* name, lv_obj_t** outName,
                          lv_obj_t** outVal, lv_obj_t** outBar) {
    lv_obj_t* t = lv_obj_create(parent);
    lv_obj_set_size(t, LV_PCT(48), LV_PCT(48));
    lv_obj_set_style_bg_color(t, lv_color_hex(0x111a2b), 0);
    lv_obj_set_style_border_color(t, lv_color_hex(0x1f2733), 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_radius(t, 10, 0);
    lv_obj_set_style_pad_all(t, 7, 0);
    lv_obj_set_style_pad_row(t, 2, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t* nm = lv_label_create(t);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(nm, lv_color_hex(0x8b97a8), 0);
    lv_label_set_text(nm, name);

    lv_obj_t* v = lv_label_create(t);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(v, lv_color_hex(0xe6edf3), 0);
    lv_label_set_text(v, "--");

    lv_obj_t* b = lv_bar_create(t);
    lv_obj_set_width(b, LV_PCT(100));
    lv_obj_set_height(b, 7);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1f2733), 0);
    lv_obj_set_style_radius(b, 4, LV_PART_INDICATOR);
    lv_obj_set_style_margin_top(b, 4, 0);

    if (outName) *outName = nm;
    *outVal = v; *outBar = b;
    return t;
}

void ViewPC::create(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_top(tab, 3, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 4, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0a0e14), 0);

    s_header = lv_label_create(tab);
    lv_obj_set_style_text_font(s_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_header, lv_color_hex(0x4fd1c5), 0);
    lv_label_set_text(s_header, "PC");

    // grid 2x2 (wrap)
    s_grid = lv_obj_create(tab);
    lv_obj_set_width(s_grid, LV_PCT(100));
    lv_obj_set_flex_grow(s_grid, 1);
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_grid, 0, 0);
    lv_obj_set_style_pad_all(s_grid, 0, 0);
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(s_grid, 6, 0);
    lv_obj_set_style_pad_column(s_grid, 6, 0);

    makeTile(s_grid, "CPU", nullptr, &s_cpuV, &s_cpuB);
    makeTile(s_grid, "RAM", nullptr, &s_ramV, &s_ramB);
    s_gpuTile = makeTile(s_grid, "GPU", &s_gpuName, &s_gpuV, &s_gpuB);
    makeTile(s_grid, "DISCO", nullptr, &s_diskV, &s_diskB);

    s_foot = lv_label_create(tab);
    lv_obj_set_style_text_font(s_foot, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_foot, lv_color_hex(0x8b97a8), 0);
    lv_obj_set_width(s_foot, LV_PCT(100));
    lv_label_set_long_mode(s_foot, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_foot, "");

    s_offline = lv_label_create(tab);
    lv_obj_set_style_text_font(s_offline, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(s_offline, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_offline, lv_color_hex(0x8b97a8), 0);
    lv_obj_set_width(s_offline, LV_PCT(100));
    lv_label_set_text(s_offline, "Agente PC offline\ninstalar pc-agent/");
    lv_obj_add_flag(s_offline, LV_OBJ_FLAG_HIDDEN);
}

static void setTile(lv_obj_t* v, lv_obj_t* b, int pct) {
    char t[8];
    snprintf(t, sizeof(t), "%d%%", pct);
    lv_label_set_text(v, t);
    setBar(b, pct);
}

void ViewPC::update(const PcMetrics& m) {
    if (!s_grid) return;
    char buf[120];

    if (!m.online) {
        lv_obj_add_flag(s_grid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_foot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_offline, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_header, "PC: sin conexion al agente");
        return;
    }
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_foot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_offline, LV_OBJ_FLAG_HIDDEN);

    snprintf(buf, sizeof(buf), "%s   %s", m.host, m.os);
    lv_label_set_text(s_header, buf);

    setTile(s_cpuV, s_cpuB, m.cpu);
    setTile(s_ramV, s_ramB, m.ram);
    if (m.disk >= 0) setTile(s_diskV, s_diskB, m.disk);
    else { lv_label_set_text(s_diskV, "n/a"); setBar(s_diskB, 0); }

    if (m.gpu >= 0) {
        lv_obj_clear_flag(s_gpuTile, LV_OBJ_FLAG_HIDDEN);
        setTile(s_gpuV, s_gpuB, m.gpu);
        lv_label_set_text(s_gpuName, m.gpu_name[0] ? m.gpu_name : "GPU");
    } else {
        lv_obj_add_flag(s_gpuTile, LV_OBJ_FLAG_HIDDEN);   // sin GPU: ocultar tile
    }

    // pie: net + cores + uptime + temps + bateria
    char down[16], up[16];
    fmtRate(m.net_down, down, sizeof(down));
    fmtRate(m.net_up, up, sizeof(up));
    int o = snprintf(buf, sizeof(buf), "NET v%s ^%s", down, up);
    if (m.cpu_count > 0) o += snprintf(buf + o, sizeof(buf) - o, "   %d nucleos", m.cpu_count);
    if (m.mem_total > 0) o += snprintf(buf + o, sizeof(buf) - o, "   RAM %.0f/%.0fG", m.mem_used, m.mem_total);
    if (m.cpu_temp >= 0) o += snprintf(buf + o, sizeof(buf) - o, "   CPU %dC", m.cpu_temp);
    if (m.gpu_temp >= 0) o += snprintf(buf + o, sizeof(buf) - o, "   GPU %dC", m.gpu_temp);
    if (m.pc_uptime_s > 0) {
        uint32_t d = m.pc_uptime_s / 86400, h = (m.pc_uptime_s % 86400) / 3600;
        o += snprintf(buf + o, sizeof(buf) - o, "   up %lud%luh", (unsigned long)d, (unsigned long)h);
    }
    if (m.batt >= 0) o += snprintf(buf + o, sizeof(buf) - o, "   Bat %d%%%s", m.batt, m.batt_plugged ? "+" : "");
    lv_label_set_text(s_foot, buf);
}
