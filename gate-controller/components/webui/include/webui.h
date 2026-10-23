/*
 * webui.h — embedded dashboard for the gate controller simulator.
 *
 * Endpoints (JSON in/out, CORS-open for local tooling):
 *   GET  /            -> index.html (embedded, gzip-friendly static)
 *   GET  /api/status  -> sim_status_json()
 *   POST /api/cmd     -> {"cmd":"open|close|stop"}          gate commands
 *   POST /api/sim     -> {"btn":"open","ms":150} | {"limit":["closed",1]}
 *                        {"i":8.5} | {"mains":false} | {"plant":true}
 *   GET  /api/log     -> recent event journal lines
 * The same shell is intended to be reused as the future BMS UI: panels are
 * data-driven from /api/status; adding a BMS means adding fields, not pages.
 */
#pragma once
#include "esp_err.h"

esp_err_t webui_start(void);   /* starts Wi-Fi (STA or open AP fallback) + httpd */
