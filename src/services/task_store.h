#pragma once
#include <stdint.h>
#include "config.h"
#include <ArduinoJson.h>

struct Task {
    char     id[TASK_ID_MAX]      = "";
    char     title[TASK_TITLE_MAX] = "";
    char     notes[TASK_NOTES_MAX] = "";
    uint8_t  priority   = 1;     // 0 low, 1 med, 2 high
    bool     done       = false;
    uint32_t created_at = 0;
    uint32_t completed_at = 0;
    uint32_t due        = 0;
};

// CRUD de tareas persistido en LittleFS:/tasks.json. Datos en RAM (hasta
// TASKS_MAX), protegidos por portMUX. Las escrituras a archivo ocurren fuera
// del lock. dirty se marca en cada cambio para que el display refresque.
namespace TaskStore {
    void begin();
    int  snapshot(Task* out, int maxN);    // copia thread-safe, devuelve n
    bool consumeDirty();                    // true si hubo cambios desde el ultimo consume

    bool add(const char* title, const char* notes, uint8_t priority, uint32_t due, Task* outCreated);
    bool setDone(const char* id, bool done);
    bool edit(const char* id, const char* title, const char* notes, int priority, long due); // -1/neg = no cambiar
    bool remove(const char* id);
    bool reorder(const char* const* ids, int n);

    void toJson(JsonArray arr);             // serializa todas las tareas
}
