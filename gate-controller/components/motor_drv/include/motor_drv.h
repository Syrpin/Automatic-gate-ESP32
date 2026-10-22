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

typedef enum {
    MDRV_IDLE, MDRV_RAMPING, MDRV_RUNNING, MDRV_DECEL, MDRV_FAULT
} mdrv_state_t;

esp_err_t motor_drv_init(void);

/* direction + throttle 0..100 (% of max duty from config). Non-blocking. */
esp_err_t motor_drv_set(gate_dir_t dir, uint8_t throttle_pct);
void      motor_drv_stop(bool soft);          /* decel then safe-state        */
mdrv_state_t motor_drv_state(void);
gate_dir_t   motor_drv_direction(void);

/* Called by gate_logic each ctrl tick to advance ramps / watchdogs. */
void motor_drv_update(const TickPeriod_t *tp);

/* Fault injection points for self-test (Phase E): */
bool motor_drv_relay_health(uint8_t *mask_out); /* K1,K2,K3 expected-vs-current */
