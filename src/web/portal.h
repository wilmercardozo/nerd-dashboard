#pragma once

// Portal cautivo de configuracion inicial (modo AP "NerdDashboard-Setup").
// Solo activo cuando no hay config valida o se solicito forzar portal.
namespace Portal {
    void start();    // levanta AP + AsyncWebServer:80 con form de setup
    void handle();   // llamar desde loop(): reinicia tras guardar (s_done)
    void stop();
    bool isDone();
}
