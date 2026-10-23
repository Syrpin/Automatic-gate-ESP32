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

/* Sensor front-end variant (easyeda/gate_v1_netlist.json: both footprints
 * on the custom board; one is fitted, selected via NVS ct_sensor):
 *   CT_SENSOR_HALL : ratiometric Hall (ACS712/ACS723 class) + RRIO mixer.
 *                    Vout = VCC/2 +/- K*I -> midrail tracks VCC, measured
 *                    at boot. DC-capable (battery/charge telemetry).
 *   CT_SENSOR_ZMCT : split-core transformer ZMCT103C + bipolar burden
 *                    R_B1/R_B2 referenced to a BUFFERED static mid-rail
 *                    (VREF divider + MCP6002 #1, docs/CT_WIRING.md).
 *                    AC only, zero offset drift, no ratiometry: midrail is
 *                    fixed by hardware, sanity-checked against VREF. */
typedef enum { CT_SENSOR_HALL = 0, CT_SENSOR_ZMCT = 1 } ct_sensor_t;

/* Front-end calibration: transfer function K [mV/A] — for HALL this is the
 * datasheet sensitivity (66/100/185 mV/A); for ZMCT it is the effective
 * burden slope V_out/I_primary (Rb/N, e.g. 22R/1000 = 22 mV/A).
 * vcc_mv: analog supply (HALL, ratiometric) or VREF rail feeding the
 * buffered mid-rail (ZMCT). i_full_a: current mapping to full ADC scale
 * (derives G when gain_x1000==0); explicit G in 0.001 units otherwise. */
void      current_sense_set_front_end(ct_sensor_t type, uint16_t k_mv_per_a,
                                      uint16_t vcc_mv, uint16_t i_full_a,
                                      uint16_t gain_x1000);
const char *current_sense_sensor_name(void);
ct_sensor_t current_sense_sensor_type(void);
/* Measure ratiometric midrail with motor OFF (64-sample average). */
esp_err_t current_sense_calibrate_midrail(void);
void      current_sense_sample(const TickPeriod_t *tp); /* DMA-fed, non-blocking */
float     current_rms(void);
/* Full-scale measurable current implied by K*G front-end budget [A]. */
float     current_sense_fullscale_a(void);
bool      current_over_run(uint32_t hold_ms);   /* sustained above i_rms_run_max */
bool      current_over_inrush(void);            /* above i_rms_peak_max ever      */
