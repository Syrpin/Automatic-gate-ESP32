/*
 * limit_sw.h — closed/open end-switches with debounce + wire-break detection.
 * NC contacts: idle HIGH; pressed LOW. Wire break (always HIGH while moving)
 * is flagged ENC-style as LIMIT_WIRE_FAULT -> triple position validation
 * (plan p.3): encoder vs switch vs time-out must agree.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"

typedef enum { SW_NONE, SW_CLOSED, SW_OPEN } sw_id_t;
typedef struct { bool at_closed, at_open, wire_fault; uint32_t changed_ms; } limit_state_t;

esp_err_t limit_sw_init(void);
void      limit_sw_scan(const TickPeriod_t *tp);   /* debounce 3 ticks x10ms */
const limit_state_t *limit_sw_state(void);
sw_id_t   limit_sw_last_event(bool *pressed);      /* edge queue for logic   */
