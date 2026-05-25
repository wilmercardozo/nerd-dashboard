#include "view_tasks.h"
#include "config.h"
#include "services/dash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static lv_obj_t* s_timer  = nullptr;
static lv_obj_t* s_mode   = nullptr;
static lv_obj_t* s_curtask = nullptr;
static lv_obj_t* s_btnTog = nullptr;
static lv_obj_t* s_btnTogL = nullptr;
static lv_obj_t* s_list   = nullptr;

static Task* s_cache = nullptr;
static int   s_n = 0;
static char  s_curId[TASK_ID_MAX] = "";

static const char* modeName(PomoMode m) {
    switch (m) {
        case POMO_WORK:  return "Trabajo";
        case POMO_SHORT: return "Descanso";
        case POMO_LONG:  return "Descanso largo";
        default:         return "Listo";
    }
}

// ── controles -> agente ─────────────────────────────────────────────────
static void onTogglePlay(lv_event_t*) {
    PomoState s = Dash::pomo();
    if (s.mode == POMO_IDLE) Dash::pomoStart("work", s_curId);
    else                     Dash::pomoPause();
}
static void onReset(lv_event_t*) { Dash::pomoReset(); }

static void onTaskClick(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_n) return;
    Dash::pomoSelect(s_cache[idx].id);   // tap = seleccionar tarea para el pomodoro
}

static void onTaskLong(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_n) return;
    Dash::taskDone(s_cache[idx].id, true);   // long-press = marcar terminada
}

static lv_obj_t* mkBtn(lv_obj_t* parent, const char* txt, lv_event_cb_t cb, lv_obj_t** outLbl) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_height(b, 32);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x18202e), 0);
    lv_obj_set_style_radius(b, 7, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(b);
    lv_obj_set_style_text_color(l, lv_color_hex(0x4fd1c5), 0);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    if (outLbl) *outLbl = l;
    return b;
}

void ViewTasks::create(lv_obj_t* tab) {
    lv_obj_set_style_pad_all(tab, 6, 0);
    lv_obj_set_style_pad_top(tab, 2, 0);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, 2, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0a0e14), 0);

    s_timer = lv_label_create(tab);
    lv_obj_set_style_text_font(s_timer, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s_timer, lv_color_hex(0xe6edf3), 0);
    lv_obj_set_width(s_timer, LV_PCT(100));
    lv_obj_set_style_text_align(s_timer, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_timer, "25:00");

    s_mode = lv_label_create(tab);
    lv_obj_set_style_text_font(s_mode, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_mode, lv_color_hex(0x8b97a8), 0);
    lv_obj_set_width(s_mode, LV_PCT(100));
    lv_obj_set_style_text_align(s_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_mode, "Listo");

    s_curtask = lv_label_create(tab);
    lv_obj_set_style_text_font(s_curtask, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_curtask, lv_color_hex(0x4fd1c5), 0);
    lv_obj_set_width(s_curtask, LV_PCT(100));
    lv_obj_set_style_text_align(s_curtask, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_curtask, "sin tarea");

    lv_obj_t* ctl = lv_obj_create(tab);
    lv_obj_set_size(ctl, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ctl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctl, 0, 0);
    lv_obj_set_style_pad_all(ctl, 0, 0);
    lv_obj_set_style_pad_top(ctl, 4, 0);
    lv_obj_clear_flag(ctl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ctl, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(ctl, 8, 0);
    s_btnTog = mkBtn(ctl, "Iniciar", onTogglePlay, &s_btnTogL);
    mkBtn(ctl, "Reset", onReset, nullptr);

    s_list = lv_list_create(tab);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x0d1320), 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 2, 0);

    s_cache = (Task*)calloc(8, sizeof(Task));
}

void ViewTasks::updatePomodoro(const PomoState& st) {
    if (!s_timer) return;
    char buf[64];
    uint32_t m = st.remaining_s / 60, s = st.remaining_s % 60;
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
    lv_label_set_text(s_timer, buf);

    snprintf(buf, sizeof(buf), "%s%s   hoy %d", modeName(st.mode),
             st.paused ? " (pausa)" : "", st.completed_today);
    lv_label_set_text(s_mode, buf);

    if (st.current_task_title[0]) {
        snprintf(buf, sizeof(buf), "Tarea: %s", st.current_task_title);
        lv_label_set_text(s_curtask, buf);
    } else {
        lv_label_set_text(s_curtask, "sin tarea (toca una abajo)");
    }
    strlcpy(s_curId, st.current_task_id, sizeof(s_curId));

    if (s_btnTogL) {
        const char* t = (st.mode == POMO_IDLE) ? "Iniciar" : (st.paused ? "Seguir" : "Pausa");
        lv_label_set_text(s_btnTogL, t);
    }
}

void ViewTasks::refresh() {
    if (!s_list || !s_cache) return;
    s_n = Dash::tasks(s_cache, 8);

    // Reconstruir solo si cambio (evita flicker/scroll reset cada poll)
    static char s_sig[320];
    char sig[320]; int o = 0;
    for (int i = 0; i < s_n; i++)
        o += snprintf(sig + o, sizeof(sig) - o, "%s%d%d|", s_cache[i].id, s_cache[i].done, s_cache[i].priority);
    snprintf(sig + o, sizeof(sig) - o, "@%s", s_curId);
    if (strcmp(sig, s_sig) == 0) return;
    strlcpy(s_sig, sig, sizeof(s_sig));

    lv_obj_clean(s_list);
    if (s_n == 0) {
        lv_obj_t* t = lv_list_add_text(s_list, "Sin tareas pendientes");
        lv_obj_set_style_text_color(t, lv_color_hex(0x5a6678), 0);
        return;
    }
    for (int i = 0; i < s_n; i++) {
        const char* sym = s_cache[i].priority == 2 ? LV_SYMBOL_WARNING
                        : s_cache[i].priority == 0 ? LV_SYMBOL_MINUS : LV_SYMBOL_RIGHT;
        lv_obj_t* btn = lv_list_add_button(s_list, sym, s_cache[i].title);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        bool sel = (strcmp(s_cache[i].id, s_curId) == 0);
        lv_obj_set_style_text_color(btn, sel ? lv_color_hex(0x4fd1c5) : lv_color_hex(0xd4deec), 0);
        lv_obj_add_event_cb(btn, onTaskClick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, onTaskLong, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
    }
}
