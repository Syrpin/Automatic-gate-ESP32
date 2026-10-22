/*
 * buttons.h — OPEN/STOP/CLOSE with debounce+long-press FSM.
 * Replaces old button.c where conditions were inverted/unreachable.
 * Emits semantic events instead of raw levels; gate_logic consumes queue.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"

typedef enum { BTN_EV_NONE, BTN_OPEN_SHORT, BTN_OPEN_LONG,
               BTN_STOP_SHORT, BTN_CLOSE_SHORT, BTN_CLOSE_LONG } btn_event_t;

esp_err_t buttons_init(void);
void      buttons_sample(const TickPeriod_t *tp);
btn_event_t buttons_pop(void);           /* one event per call, FIFO depth 4 */
bool      buttons_any_pressed(void);     /* light-sleep wake decision         */
