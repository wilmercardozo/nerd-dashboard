#pragma once
#include <lvgl.h>
#include "services/dash.h"   // ServiceItem

// Vista 1 — Servicios (monitor tipo uptime). Lista los monitores del agente;
// los caidos/degradados van primero (el agente ya los ordena).
namespace ViewClaude {
    void create(lv_obj_t* tab);
    void update(const ServiceItem* svc, int n);
}
