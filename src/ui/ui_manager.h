#pragma once
#include <stdint.h>

// Gestor de UI LVGL: init de display + touch, tabview de 4 tabs, barra de
// estado inferior, y bucle tick() que corre en la tarea UI (core 0).
namespace UIManager {
    void init();                  // init display + touch + construye UI (sin tarea)
    void tick();                  // llamar desde taskUI: consume datos + lv_timer_handler
    void pumpLvgl(uint32_t ms);   // spin de lv_timer_handler durante boot (desde core 1)
    void setStatus(const char* msg); // texto en barra inferior (mensajes de arranque)
}
