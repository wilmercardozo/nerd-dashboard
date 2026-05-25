#include "view_claude.h"
#include <string.h>
#include <stdio.h>

static lv_obj_t* s_header = nullptr;
static lv_obj_t* s_dot[DASH_MAX_SERVICES]  = {nullptr};
static lv_obj_t* s_name[DASH_MAX_SERVICES] = {nullptr};
static lv_obj_t* s_stat[DASH_MAX_SERVICES] = {nullptr};
static lv_obj_t* s_row[DASH_MAX_SERVICES]  = {nullptr};
static lv_obj_t* s_empty = nullptr;

static lv_color_t statColor(const char* s) {
    if (!strcmp(s, "up"))       return lv_color_hex(0x54d98c);
    if (!strcmp(s, "degraded")) return lv_color_hex(0xf5c542);
    if (!strcmp(s, "down"))     return lv_color_hex(0xff5d6c);
    return lv_color_hex(0x6b7a90);
}
static const char* statText(const char* s) {
    if (!strcmp(s, "up"))       return "OK";
    if (!strcmp(s, "degraded")) return "DEGR.";
    if (!strcmp(s, "down"))     return "CAIDO";
    return "...";
}

void ViewClaude::create(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_top(tab, 4, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 2, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0a0e14), 0);

    s_header = lv_label_create(tab);
    lv_obj_set_style_text_font(s_header, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_header, lv_color_hex(0x8b97a8), 0);
    lv_label_set_text(s_header, "SERVICIOS");

    for (int i = 0; i < DASH_MAX_SERVICES; i++) {
        lv_obj_t* r = lv_obj_create(tab);
        lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r, 0, 0);
        lv_obj_set_style_border_color(r, lv_color_hex(0x131c2b), 0);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        lv_obj_set_style_pad_ver(r, 5, 0);
        lv_obj_set_style_pad_hor(r, 2, 0);
        lv_obj_set_style_radius(r, 0, 0);
        lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, 9, 0);

        s_dot[i] = lv_obj_create(r);
        lv_obj_set_size(s_dot[i], 12, 12);
        lv_obj_set_style_radius(s_dot[i], 6, 0);
        lv_obj_set_style_border_width(s_dot[i], 0, 0);
        lv_obj_clear_flag(s_dot[i], LV_OBJ_FLAG_SCROLLABLE);

        s_name[i] = lv_label_create(r);
        lv_obj_set_style_text_font(s_name[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_name[i], lv_color_hex(0xe6edf3), 0);
        lv_obj_set_flex_grow(s_name[i], 1);          // ocupa el espacio libre
        lv_obj_set_width(s_name[i], 1);              // base 0 para que grow lo dimensione
        lv_label_set_long_mode(s_name[i], LV_LABEL_LONG_DOT);  // trunca con ... si no cabe

        s_stat[i] = lv_label_create(r);
        lv_obj_set_style_text_font(s_stat[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_min_width(s_stat[i], 52, 0);          // ancho fijo para el estado
        lv_obj_set_style_text_align(s_stat[i], LV_TEXT_ALIGN_RIGHT, 0);

        s_row[i] = r;
        lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN);
    }

    s_empty = lv_label_create(tab);
    lv_obj_set_style_text_color(s_empty, lv_color_hex(0x5a6678), 0);
    lv_label_set_text(s_empty, "Sin servicios. Configura en el agente PC.");
}

void ViewClaude::update(const ServiceItem* svc, int n) {
    if (!s_header) return;
    int down = 0;
    for (int i = 0; i < n; i++)
        if (!strcmp(svc[i].status, "down") || !strcmp(svc[i].status, "degraded")) down++;

    char hdr[40];
    if (down > 0) {
        snprintf(hdr, sizeof(hdr), "SERVICIOS  -  %d CON FALLAS", down);
        lv_obj_set_style_text_color(s_header, lv_color_hex(0xff5d6c), 0);
    } else {
        snprintf(hdr, sizeof(hdr), "SERVICIOS  -  %d OK", n);
        lv_obj_set_style_text_color(s_header, lv_color_hex(0x54d98c), 0);
    }
    lv_label_set_text(s_header, hdr);

    lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    if (n == 0) lv_obj_clear_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < DASH_MAX_SERVICES; i++) {
        if (i < n) {
            lv_obj_clear_flag(s_row[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(s_dot[i], statColor(svc[i].status), 0);
            lv_obj_set_style_bg_opa(s_dot[i], LV_OPA_COVER, 0);
            lv_label_set_text(s_name[i], svc[i].name);
            lv_obj_set_style_text_color(s_stat[i], statColor(svc[i].status), 0);
            lv_label_set_text(s_stat[i], statText(svc[i].status));
        } else {
            lv_obj_add_flag(s_row[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
