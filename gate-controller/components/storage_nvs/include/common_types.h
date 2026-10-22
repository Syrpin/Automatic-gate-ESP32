/*
 * common_types.h — shared small types used across components.
 * TickPeriod_t replaces the old global `timerClock` switch-based scheduler.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t period_ms;   /* caller task period            */
    uint32_t now_ms;        /* esp_timer_get_time()/1000 at entry  */
} TickPeriod_t;

typedef enum {
    DIR_NONE = 0,
    DIR_OPEN = 1,
    DIR_CLOSE = -1,
} gate_dir_t;
