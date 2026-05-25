#include "view_usage.h"
#include <stdio.h>
#include <string.h>

// Solo ASCII en labels (Montserrat no trae em-dash/middot).
static lv_obj_t* s_status = nullptr;
static lv_obj_t* s_a5 = nullptr, *s_a5v = nullptr, *s_r5 = nullptr;  // gauge 5h
static lv_obj_t* s_a7 = nullptr, *s_a7v = nullptr, *s_r7 = nullptr;  // gauge semanal

static lv_color_t levelColor(int pct) {
    if (pct >= 90) return lv_color_hex(0xff5d5d);
    if (pct >= 70) return lv_color_hex(0xf5c542);
    return lv_color_hex(0x4fd1c5);
}
static void fmtReset(uint32_t totalMin, char* out, size_t n) {
    if (totalMin >= 1440) snprintf(out, n, "reset %lud %luh", (unsigned long)(totalMin / 1440), (unsigned long)((totalMin % 1440) / 60));
    else if (totalMin >= 60) snprintf(out, n, "reset %luh %02lum", (unsigned long)(totalMin / 60), (unsigned long)(totalMin % 60));
    else snprintf(out, n, "reset %lum", (unsigned long)totalMin);
}
static lv_obj_t* lab(lv_obj_t* p, const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

// spark estilo Claude (8 rayos), con pulso
static lv_point_precise_t s_sparkPts[16] = {
    {9,9},{18,9},{9,9},{15,3},{9,9},{9,0},{9,9},{3,3},
    {9,9},{0,9},{9,9},{3,15},{9,9},{9,18},{9,9},{15,15}
};
static void sparkPulse(void* o, int32_t v) { lv_obj_set_style_opa((lv_obj_t*)o, (lv_opa_t)v, 0); }
static void makeSpark(lv_obj_t* parent) {
    lv_obj_t* ln = lv_line_create(parent);
    lv_line_set_points(ln, s_sparkPts, 16);
    lv_obj_set_size(ln, 18, 18);
    lv_obj_set_style_line_color(ln, lv_color_hex(0xD97757), 0);
    lv_obj_set_style_line_width(ln, 2, 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a, ln);
    lv_anim_set_exec_cb(&a, sparkPulse);
    lv_anim_set_values(&a, 110, 255);
    lv_anim_set_duration(&a, 1300);
    lv_anim_set_playback_duration(&a, 1300);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

// gauge circular: anillo + % al centro, nombre y reset debajo
static lv_obj_t* makeGauge(lv_obj_t* parent, const char* name, lv_obj_t** outVal, lv_obj_t** outReset) {
    lv_obj_t* col = lv_obj_create(parent);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, 2, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* arc = lv_arc_create(col);
    lv_obj_set_size(arc, 104, 104);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x1f2733), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x4fd1c5), LV_PART_INDICATOR);

    lv_obj_t* v = lv_label_create(arc);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(v, lv_color_hex(0xe6edf3), 0);
    lv_label_set_text(v, "--");
    lv_obj_center(v);

    lv_obj_t* nm = lab(col, &lv_font_montserrat_14, 0xe6edf3);
    lv_label_set_text(nm, name);
    lv_obj_t* rs = lab(col, &lv_font_montserrat_12, 0x6b7a90);
    lv_label_set_text(rs, "");

    *outVal = v; *outReset = rs;
    return arc;
}

void ViewUsage::create(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_top(tab, 4, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 4, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0a0e14), 0);

    // ── cabecera: spark + titulo + pill ────────────────────────────
    lv_obj_t* head = lv_obj_create(tab);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* left = lv_obj_create(head);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 8, 0);
    makeSpark(left);
    lv_obj_t* title = lab(left, &lv_font_montserrat_14, 0xe6edf3);
    lv_label_set_text(title, "Claude Pro/Max");

    s_status = lab(head, &lv_font_montserrat_12, 0x8b97a8);
    lv_obj_set_style_pad_hor(s_status, 8, 0);
    lv_obj_set_style_pad_ver(s_status, 2, 0);
    lv_obj_set_style_radius(s_status, 8, 0);
    lv_obj_set_style_bg_opa(s_status, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_status, lv_color_hex(0x1f2733), 0);
    lv_label_set_text(s_status, "OFFLINE");

    // ── cuerpo: dos gauges (5h + semanal) ──────────────────────────
    lv_obj_t* body = lv_obj_create(tab);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_a5 = makeGauge(body, "Ventana 5h", &s_a5v, &s_r5);
    s_a7 = makeGauge(body, "Semana",     &s_a7v, &s_r7);
}

static void setGauge(lv_obj_t* arc, lv_obj_t* val, lv_obj_t* rs, bool ok, int pct, uint32_t resetMin) {
    char buf[24];
    if (!ok) {
        lv_arc_set_value(arc, 0);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x1f2733), LV_PART_INDICATOR);
        lv_label_set_text(val, "--");
        lv_obj_set_style_text_color(val, lv_color_hex(0x6b7a90), 0);
        lv_label_set_text(rs, "sin datos");
        return;
    }
    lv_arc_set_value(arc, pct);
    lv_obj_set_style_arc_color(arc, levelColor(pct), LV_PART_INDICATOR);
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(val, buf);
    lv_obj_set_style_text_color(val, levelColor(pct), 0);
    fmtReset(resetMin, buf, sizeof(buf));
    lv_label_set_text(rs, buf);
}

void ViewUsage::update(const ClaudeUsage& u) {
    if (!s_a5) return;

    // pill de estado
    const char* stTxt = "OFFLINE";
    lv_color_t stBg = lv_color_hex(0x1f2733), stFg = lv_color_hex(0x8b97a8);
    if (u.rate_ok) {
        if (strncmp(u.rate_status, "allowed_warning", 15) == 0) { stTxt = "ALERTA"; stBg = lv_color_hex(0x3a3416); stFg = lv_color_hex(0xf5c542); }
        else if (strncmp(u.rate_status, "allowed", 7) == 0)      { stTxt = "OK";     stBg = lv_color_hex(0x16301f); stFg = lv_color_hex(0x5dd87a); }
        else                                                      { stTxt = "BLOQUEADO"; stBg = lv_color_hex(0x3a1620); stFg = lv_color_hex(0xff5d5d); }
    }
    lv_label_set_text(s_status, stTxt);
    lv_obj_set_style_bg_color(s_status, stBg, 0);
    lv_obj_set_style_text_color(s_status, stFg, 0);

    setGauge(s_a5, s_a5v, s_r5, u.rate_ok, u.rate_5h_pct, u.rate_5h_reset_min);
    setGauge(s_a7, s_a7v, s_r7, u.rate_ok, u.rate_7d_pct, u.rate_7d_reset_min);
}
