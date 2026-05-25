#include "view_usage.h"
#include <stdio.h>
#include <string.h>

// Solo ASCII en labels (Montserrat del display no trae em-dash/middot).

static lv_obj_t* s_status = nullptr;
static lv_obj_t* s_l5 = nullptr, *s_b5 = nullptr, *s_sub5 = nullptr;
static lv_obj_t* s_l7 = nullptr, *s_b7 = nullptr, *s_sub7 = nullptr;
static lv_obj_t* s_sem = nullptr;

static lv_color_t levelColor(int pct) {
    if (pct >= 90) return lv_color_hex(0xff5d5d);
    if (pct >= 70) return lv_color_hex(0xf5c542);
    return lv_color_hex(0x4fd1c5);
}
static void fmtMin(uint32_t totalMin, char* out, size_t n) {
    if (totalMin >= 1440) snprintf(out, n, "%lud %luh", (unsigned long)(totalMin / 1440), (unsigned long)((totalMin % 1440) / 60));
    else if (totalMin >= 60) snprintf(out, n, "%luh %02lum", (unsigned long)(totalMin / 60), (unsigned long)(totalMin % 60));
    else snprintf(out, n, "%lu min", (unsigned long)totalMin);
}
static lv_obj_t* meter(lv_obj_t* parent) {
    lv_obj_t* b = lv_bar_create(parent);
    lv_obj_set_size(b, LV_PCT(100), 9);
    lv_bar_set_range(b, 0, 100);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1f2733), 0);
    lv_obj_set_style_radius(b, 4, LV_PART_INDICATOR);
    return b;
}
static void setMeter(lv_obj_t* b, int pct) {
    int v = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
    lv_bar_set_value(b, v, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(b, levelColor(pct), LV_PART_INDICATOR);
}
static lv_obj_t* lab(lv_obj_t* p, const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

void ViewUsage::create(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_top(tab, 4, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 3, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0a0e14), 0);

    // cabecera + pill
    lv_obj_t* head = lv_obj_create(tab);
    lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = lab(head, &lv_font_montserrat_14, 0x8b97a8);
    lv_label_set_text(title, "LIMITE Pro/Max");
    s_status = lab(head, &lv_font_montserrat_12, 0x8b97a8);
    lv_obj_set_style_pad_hor(s_status, 8, 0);
    lv_obj_set_style_pad_ver(s_status, 2, 0);
    lv_obj_set_style_radius(s_status, 8, 0);
    lv_obj_set_style_bg_opa(s_status, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_status, lv_color_hex(0x1f2733), 0);
    lv_label_set_text(s_status, "OFFLINE");

    // 5h
    s_l5 = lab(tab, &lv_font_montserrat_14, 0xe6edf3);
    lv_label_set_text(s_l5, "5h    --");
    s_b5 = meter(tab);
    s_sub5 = lab(tab, &lv_font_montserrat_12, 0x6b7a90);
    lv_label_set_text(s_sub5, "");

    // semanal
    s_l7 = lab(tab, &lv_font_montserrat_14, 0xe6edf3);
    lv_label_set_text(s_l7, "Semana  --");
    s_b7 = meter(tab);
    s_sub7 = lab(tab, &lv_font_montserrat_12, 0x6b7a90);
    lv_label_set_text(s_sub7, "");

    // semaforo
    s_sem = lab(tab, &lv_font_montserrat_14, 0x8b97a8);
    lv_obj_set_style_text_align(s_sem, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_sem, LV_PCT(100));
    lv_label_set_text(s_sem, "...");
}

void ViewUsage::update(const ClaudeUsage& u) {
    if (!s_l5) return;
    char buf[96], rs[24];

    // pill estado
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

    if (!u.rate_ok) {
        lv_label_set_text(s_l5, "5h    Agente PC offline");
        lv_label_set_text(s_sub5, "configurar IP del PC en /");
        lv_label_set_text(s_l7, "Semana  --");
        lv_label_set_text(s_sub7, "");
        setMeter(s_b5, 0); setMeter(s_b7, 0);
    } else {
        // 5h: % usado / libre + cuanto falta
        snprintf(buf, sizeof(buf), "5h     %d%% usado   queda %d%%", u.rate_5h_pct, 100 - u.rate_5h_pct);
        lv_label_set_text(s_l5, buf);
        setMeter(s_b5, u.rate_5h_pct);
        fmtMin(u.rate_5h_reset_min, rs, sizeof(rs));
        snprintf(buf, sizeof(buf), "se restablece en %s", rs);
        lv_label_set_text(s_sub5, buf);

        // semanal
        snprintf(buf, sizeof(buf), "Semana %d%% usado   queda %d%%", u.rate_7d_pct, 100 - u.rate_7d_pct);
        lv_label_set_text(s_l7, buf);
        setMeter(s_b7, u.rate_7d_pct);
        fmtMin(u.rate_7d_reset_min, rs, sizeof(rs));
        snprintf(buf, sizeof(buf), "se restablece en %s", rs);
        lv_label_set_text(s_sub7, buf);
    }

    // semaforo: peor de los dos porcentajes
    int worst = u.rate_ok ? u.rate_5h_pct : 0;
    if (u.rate_ok && u.rate_7d_pct > worst) worst = u.rate_7d_pct;
    lv_color_t col; const char* txt;
    if (worst >= 90)      { col = lv_color_hex(0xff5d5d); txt = "LIMITE - frena un poco"; }
    else if (worst >= 70) { col = lv_color_hex(0xf5c542); txt = "Cerca del limite"; }
    else                  { col = lv_color_hex(0x5dd87a); txt = "Todo OK"; }
    lv_obj_set_style_text_color(s_sem, col, 0);
    lv_label_set_text(s_sem, txt);
}
