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

/* Front-end calibration (SCHEMATIC.md §2): sensor gain K [mV/A], analog
 * supply [mV], full-scale current [A] (derives G if gain_x1000==0),
 * explicit G in 0.001 units. Call once at boot from NVS config. */
void      current_sense_set_front_end(uint16_t k_mv_per_a, uint16_t vcc_mv,
                                      uint16_t i_full_a, uint16_t gain_x1000);
/* Measure ratiometric midrail with motor OFF (64-sample average). */
esp_err_t current_sense_calibrate_midrail(void);
void      current_sense_sample(const TickPeriod_t *tp); /* DMA-fed, non-blocking */
float     current_rms(void);
/* Full-scale measurable current implied by K*G front-end budget [A]. */
float     current_sense_fullscale_a(void);
bool      current_over_run(uint32_t hold_ms);   /* sustained above i_rms_run_max */
bool      current_over_inrush(void);            /* above i_rms_peak_max ever      */
