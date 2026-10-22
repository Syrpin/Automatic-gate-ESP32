/*
 * events_log.h — RAM ring buffer + batched JSONL journal (separate NVS
 * partition "log"), flush by telemetry task; power_mgmt forces flush inside
 * the capacitor hold-up window (plan p.9.1 step 3).
 */
#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum { EV_INFO, EV_WARN, EV_MOVE, EV_FAULT, EV_VISION, EV_POWER } ev_kind_t;

esp_err_t events_log_init(void);
void      events_log_write(ev_kind_t k, const char *fmt, ...);
void      events_log_flush(void);           /* batched write to flash         */
uint32_t  events_log_dropped(void);         /* ring overflow counter          */
