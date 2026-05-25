#pragma once
#include <stdint.h>
#include "config.h"

enum PomoMode { POMO_IDLE = 0, POMO_WORK, POMO_SHORT, POMO_LONG };

struct PomoConfig {
    uint16_t work_s    = 1500;   // 25 min
    uint16_t short_s   = 300;    // 5 min
    uint16_t long_s    = 900;    // 15 min
    uint8_t  long_every = 4;     // descanso largo cada N trabajos
};

struct PomoState {
    PomoMode mode       = POMO_IDLE;
    uint32_t duration_s = 0;
    uint32_t remaining_s = 0;     // computado
    bool     paused     = false;
    char     current_task_id[TASK_ID_MAX] = "";
    char     current_task_title[48] = "";
    uint8_t  completed_today = 0;
    PomoConfig cfg;
};

namespace Pomodoro {
    void begin();
    void tick();                  // detecta fin de periodo; llamar seguido
    PomoState state();            // copia con remaining_s computado
    bool consumeDirty();
    void start(PomoMode mode, const char* taskId);
    void pause();
    void resume();
    void reset();
    void setConfig(int work_s, int short_s, int long_s, int long_every); // <0 = sin cambio
}
