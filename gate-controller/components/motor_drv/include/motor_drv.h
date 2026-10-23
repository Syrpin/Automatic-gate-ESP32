/*
 * motor_drv.h — triac phase-angle + direction relays (parallel "симистор-реле" node).
 *
 * Fixes vs old motor.c:
 *  - no blocking while-loops: every transition is timer-driven;
 *  - relay sequencing with dead-time BEFORE triac firing (inductive kick guard);
 *  - stuck-relay detection via current cross-check (current_sense feeds back);
 *  - soft-start (ramp_up) and ramp-down before limits handled by gate_logic,
 *    this layer only accepts an angle setpoint.
 *
 * Firing control loop runs in a dedicated high-priority task synchronized to
 * the mains zero-cross (PIN_MAINS_DETECT ext interrupt), NOT in ctrl_task.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"
#include "storage_nvs.h"   /* mains_freq_sel_t (regional 50/60 Hz selection) */

typedef enum {
    MDRV_IDLE, MDRV_RAMPING, MDRV_RUNNING, MDRV_DECEL, MDRV_FAULT
} mdrv_state_t;

esp_err_t motor_drv_init(void);

/* direction + throttle 0..100 (% of max duty from config). Non-blocking. */
esp_err_t motor_drv_set(gate_dir_t dir, uint8_t throttle_pct);
void      motor_drv_stop(bool soft);          /* decel then safe-state        */
mdrv_state_t motor_drv_state(void);
gate_dir_t   motor_drv_direction(void);

/* Regional mains frequency (50/60 Hz) for the phase-angle firing loop.
 * Set from gate_config.mains_freq at init; sim/web can change live.
 * Returns the effective half-period in us (20000/16667/10000/8333). */
void     motor_drv_set_mains_freq(mains_freq_sel_t sel);
mains_freq_sel_t motor_drv_mains_freq(void);   /* current selection         */
uint16_t motor_drv_measured_hz(void);          /* last ZC-measured freq, or 0 */
uint32_t motor_drv_half_period_us(void);       /* effective half-period us:
                                                  50Hz=10000, 60Hz=8333; in AUTO
                                                  tracks the measured line period */

/* Called by gate_logic each ctrl tick to advance ramps / watchdogs. */
void motor_drv_update(const TickPeriod_t *tp);

/* Fault injection points for self-test (Phase E): */
bool motor_drv_relay_health(uint8_t *mask_out); /* K1,K2,K3 expected-vs-current */
