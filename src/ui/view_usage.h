#pragma once
#include <lvgl.h>
#include "services/claude_usage.h"

// Vista 2 — Tokens (Claude.ai ventana 5h + API/Code via Admin API + semaforo).
namespace ViewUsage {
    void create(lv_obj_t* tab);
    void update(const ClaudeUsage& u);
}
