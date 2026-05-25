#include "view_pc.h"
#include <stdio.h>

static lv_obj_t* s_header = nullptr;
static lv_obj_t* s_body   = nullptr;
static lv_obj_t* s_arc    = nullptr;
static lv_obj_t* s_arcLbl = nullptr;
static lv_obj_t* s_ram    = nullptr, *s_ramB  = nullptr;
static lv_obj_t* s_gpu    = nullptr, *s_gpuB  = nullptr;
static lv_obj_t* s_disk   = nullptr, *s_diskB = nullptr;
static lv_obj_t* s_foot   = nullptr;
static lv_obj_t* s_offline = nullptr;

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

static void barRow(lv_obj_t* parent, lv_obj_t** outLabel, lv_obj_t** outBar) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xc9d4e3), 0);
    lv_label_set_text(l, "");
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_set_size(b, LV_PCT(100), 8);
    lv_bar_set_range(b, 0, 100);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1f2733), 0);
    lv_obj_set_style_radius(b, 4, LV_PART_INDICATOR);
    *outLabel = l; *outBar = b;
}
static void setBar(lv_obj_t* b, int pct) {
    int v = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    lv_bar_set_value(b, v, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(b, lvlColor(pct), LV_PART_INDICATOR);
}

void ViewPC::create(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_top(tab, 4, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 4, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0a0e14), 0);

    s_header = lv_label_create(tab);
    lv_obj_set_style_text_font(s_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_header, lv_color_hex(0x4fd1c5), 0);
    lv_label_set_text(s_header, "PC");

    s_body = lv_obj_create(tab);
    lv_obj_set_size(s_body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_body, 0, 0);
    lv_obj_set_style_pad_all(s_body, 0, 0);
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_body, 12, 0);

    s_arc = lv_arc_create(s_body);
    lv_obj_set_size(s_arc, 84, 84);
    lv_arc_set_rotation(s_arc, 135);
    lv_arc_set_bg_angles(s_arc, 0, 270);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x1f2733), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x4fd1c5), LV_PART_INDICATOR);
    s_arcLbl = lv_label_create(s_arc);
    lv_obj_set_style_text_font(s_arcLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_arcLbl, lv_color_hex(0xe6edf3), 0);
    lv_label_set_text(s_arcLbl, "CPU");
    lv_obj_center(s_arcLbl);

    lv_obj_t* col = lv_obj_create(s_body);
    lv_obj_set_size(col, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);
    barRow(col, &s_ram, &s_ramB);
    barRow(col, &s_gpu, &s_gpuB);
    barRow(col, &s_disk, &s_diskB);

    s_foot = lv_label_create(tab);
    lv_obj_set_style_text_font(s_foot, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_foot, lv_color_hex(0x8b97a8), 0);
    lv_obj_set_width(s_foot, LV_PCT(100));
    lv_label_set_text(s_foot, "");

    s_offline = lv_label_create(tab);
    lv_obj_set_style_text_font(s_offline, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(s_offline, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_offline, lv_color_hex(0x8b97a8), 0);
    lv_obj_set_width(s_offline, LV_PCT(100));
    lv_label_set_text(s_offline, "Agente PC offline\ninstalar pc-agent/");
    lv_obj_add_flag(s_offline, LV_OBJ_FLAG_HIDDEN);
}

void ViewPC::update(const PcMetrics& m) {
    if (!s_arc) return;
    char buf[120];

    if (!m.online) {
        lv_obj_add_flag(s_body, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_foot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_offline, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_header, "PC: sin conexion al agente");
        return;
    }
    lv_obj_clear_flag(s_body, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_foot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_offline, LV_OBJ_FLAG_HIDDEN);

    snprintf(buf, sizeof(buf), "%s   %s", m.host, m.os);
    lv_label_set_text(s_header, buf);

    lv_arc_set_value(s_arc, m.cpu);
    lv_obj_set_style_arc_color(s_arc, lvlColor(m.cpu), LV_PART_INDICATOR);
    snprintf(buf, sizeof(buf), "%d%%", m.cpu);
    lv_label_set_text(s_arcLbl, buf);

    // RAM (con GB si disponible)
    if (m.mem_total > 0)
        snprintf(buf, sizeof(buf), "RAM  %d%%   %.1f / %.1f GB", m.ram, m.mem_used, m.mem_total);
    else
        snprintf(buf, sizeof(buf), "RAM  %d%%", m.ram);
    lv_label_set_text(s_ram, buf);
    setBar(s_ramB, m.ram);

    // GPU: ocultar fila si no hay (en vez de mostrar n/a)
    if (m.gpu >= 0) {
        lv_obj_clear_flag(s_gpu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_gpuB, LV_OBJ_FLAG_HIDDEN);
        if (m.gpu_name[0]) snprintf(buf, sizeof(buf), "GPU  %d%%   %.10s", m.gpu, m.gpu_name);
        else               snprintf(buf, sizeof(buf), "GPU  %d%%", m.gpu);
        lv_label_set_text(s_gpu, buf);
        setBar(s_gpuB, m.gpu);
    } else {
        lv_obj_add_flag(s_gpu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_gpuB, LV_OBJ_FLAG_HIDDEN);
    }

    // DISK
    if (m.disk >= 0) {
        lv_obj_clear_flag(s_disk, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_diskB, LV_OBJ_FLAG_HIDDEN);
        snprintf(buf, sizeof(buf), "DISK %d%%", m.disk);
        lv_label_set_text(s_disk, buf);
        setBar(s_diskB, m.disk);
    } else {
        lv_obj_add_flag(s_disk, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_diskB, LV_OBJ_FLAG_HIDDEN);
    }

    // pie: cores/freq + uptime + temps + net + bateria (dos lineas)
    char l1[64] = "", l2[80] = "";
    int o1 = 0;
    if (m.cpu_count > 0) o1 += snprintf(l1 + o1, sizeof(l1) - o1, "%d nucleos", m.cpu_count);
    if (m.cpu_freq > 0)  o1 += snprintf(l1 + o1, sizeof(l1) - o1, " %dMHz", m.cpu_freq);
    if (m.pc_uptime_s > 0) {
        uint32_t d = m.pc_uptime_s / 86400, h = (m.pc_uptime_s % 86400) / 3600;
        o1 += snprintf(l1 + o1, sizeof(l1) - o1, "  up %lud%luh", (unsigned long)d, (unsigned long)h);
    }
    char down[16], up[16];
    fmtRate(m.net_down, down, sizeof(down));
    fmtRate(m.net_up, up, sizeof(up));
    int o2 = snprintf(l2, sizeof(l2), "NET v%s ^%s", down, up);
    if (m.cpu_temp >= 0) o2 += snprintf(l2 + o2, sizeof(l2) - o2, "  CPU %dC", m.cpu_temp);
    if (m.gpu_temp >= 0) o2 += snprintf(l2 + o2, sizeof(l2) - o2, "  GPU %dC", m.gpu_temp);
    if (m.batt >= 0)     o2 += snprintf(l2 + o2, sizeof(l2) - o2, "  Bat %d%%%s", m.batt, m.batt_plugged ? "+" : "");
    snprintf(buf, sizeof(buf), "%s\n%s", l1, l2);
    lv_label_set_text(s_foot, buf);
}
