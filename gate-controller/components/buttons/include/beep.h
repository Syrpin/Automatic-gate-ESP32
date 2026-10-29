/*
 * beep.h — non-blocking buzzer patterns (replacement for old blocking beep()).
 * Driven by beep_update(dt_ms) from the 50 ms beep_task; patterns are data,
 * never busy-wait (old code held the CPU with Delay_ms inside ISR-adjacent loops).
 */
#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t beep_init(void);
void beep_update(uint32_t dt_ms);      /* call every ~50ms with real dt   */
void beep_short(void);                 /* key press feedback              */
void beep_confirm(void);               /* command accepted                */
void beep_warn(void);                  /* recoverable problem             */
void beep_error(void);                 /* latched fault                   */
void beep_move(void);                  /* looping tick while gate travels */
void beep_fault_loop(void);            /* looping error while GS_FAULT    */
void beep_stop_pattern(void);          /* cancel looping patterns         */
/* raw pattern start (custom sequences in wizard): NULL = stop */
void beep_start(const uint8_t *pattern);
