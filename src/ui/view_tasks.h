#pragma once
#include <lvgl.h>
#include "services/pomodoro.h"
#include "services/task_store.h"

// Vista 4 — Pomodoro + lista de tareas.
namespace ViewTasks {
    void create(lv_obj_t* tab);
    void updatePomodoro(const PomoState& st);   // refresca timer/estado (~1Hz)
    void refresh();                              // recarga lista desde TaskStore
}
