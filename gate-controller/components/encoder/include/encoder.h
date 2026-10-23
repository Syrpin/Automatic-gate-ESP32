/*
 * encoder.h — position feedback for three sensor families (plan p.2, p.11.1).
 *  - OPTICAL: quadrature A/B via PCNT (custom PCB) or GPIO-ISR decode (devkit).
 *  - DX (реед-диск): single channel; direction resolved from motor command
 *    + inter-pulse gap sanity.
 *  - DI (индуктивный): pulse train on one line; same dir strategy, plus
 *    tooth-count window filtering (edge jitter is its signature flaw).
 * Fixes vs old code: int32 accumulator with overflow-safe delta math
 * (old TIM3->CNT wrap bug), configurable enc_min as integer counts.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"
#include "storage_nvs.h"

typedef enum { ENC_OK, ENC_STALE, ENC_CONTRADICTION } enc_status_t;

esp_err_t encoder_init(void);                 /* picks profile from cfg      */
void      encoder_update(const TickPeriod_t *tp);
int32_t   encoder_position(void);             /* counts, 0=closed            */
enc_status_t encoder_status(void);
uint32_t  encoder_last_change_ms(void);       /* staleness watchdog input    */
void      encoder_set_origin_closed(void);    /* calibration at CL limit     */
void      encoder_set_origin_open(void);      /* calibration at OP limit     */
float     encoder_speed_counts_per_s(void);   /* for pinch & ramp-down calc  */
/* Cross-check hook used by gate_logic: does motion agree with commanded dir? */
bool      encoder_dir_matches(gate_dir_t cmd, uint32_t window_ms);
void      encoder_set_position_raw(int32_t p); /* trusted restore from NVS */
#ifdef CONFIG_GATE_SIM
/* simulator feed: add counts as if the sensor produced them (HIL tests) */
void      encoder_add_sim_counts(int32_t d);
#endif
