#pragma once
#include <lvgl.h>
#include "services/pc_metrics.h"

// Vista 3 — Monitor del PC (agente Python).
namespace ViewPC {
    void create(lv_obj_t* tab);
    void update(const PcMetrics& m);
}
