/*
 * current_sense.h — CT clamp RMS over integer mains cycles (fixes old adc.c
 * single-sample peak hack and float IMAX). Provides:
 *  - i_rms for stall/pinch detection;
 *  - inrush window awareness (cfg->inrush_window_ms);
 *  - no-current watchdog input for motor_drv fault cross-check.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"

esp_err_t current_sense_init(void);
void      current_sense_sample(const TickPeriod_t *tp); /* DMA-fed, non-blocking */
float     current_rms(void);
bool      current_over_run(uint32_t hold_ms);   /* sustained above i_rms_run_max */
bool      current_over_inrush(void);            /* above i_rms_peak_max ever      */
/* Calibration hook (wizard, Phase E): amps-per-LSB of the centered window.
   Until set >0, overcurrent protections stay inactive (fail-safe, logged). */
void      current_sense_set_gain(float amps_per_lsb);
