/*
 * sim.h — hardware-in-the-loop simulator + test harness (bare-board testing).
 *
 * Runs the REAL logic stack (gate_logic FSM, motor_drv relay/ramp FSM,
 * buttons/limit debounce, encoder accumulation, current model, power budget)
 * with zero physical peripherals. Two transports feed one API:
 *   - USB-UART console  (devkit built-in USB-Serial-JTAG / UART0 REPL)
 *   - Wi-Fi web UI      (browser dashboard; shell reused later as BMS UI)
 *
 * Enable at build time: CONFIG_GATE_SIM=y
 *   idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.sim.defaults" build
 *
 * On a bare board ADC reads float — sim_current_inject() overrides the CT RMS
 * pipeline so pinch/stall tests work without a clamp on the wire.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Start simulation engine (plant-model task) + console REPL on USB-CDC/UART0.
 * Called from app_main when CONFIG_GATE_SIM is set. */
esp_err_t sim_start(void);

/* --- virtual-peripheral injection (REPL, web UI and host unit tests) ------ */
void sim_btn_press(int idx, uint32_t ms);      /* 0=OPEN 1=STOP 2=CLOSE      */
void sim_limit_set(int id, bool engaged);      /* 0=CLOSED-limit 1=OPEN-limit*/
void sim_mains_set(bool ok);                   /* loss/restore of mains      */
void sim_enc_add(int32_t d);                   /* raw counts delta           */
void sim_current_inject(float amps);           /* override CT RMS input      */
bool sim_current_override_active(void);

/* Plant model: maps motor drive state -> encoder pulses + current profile.
 * Called by sim task at 10 ms. */
void sim_plant_step(uint32_t dt_ms);
void sim_plant_model_enable(bool on);

/* Virtual mains zero-cross generator (regional 50/60 Hz bench test):
 * feeds motor_drv_zero_cross_us() so phase-angle timing works with no
 * transformer wired. Started via console "ac zc 50|60" or web UI. */
void sim_zc_start(uint16_t hz);          /* 50 or 60                          */
void sim_zc_stop(void);
bool sim_zc_running(void);

/* JSON status snapshot for web UI / telemetry bridge. Needs buf >= 1024 B. */
int sim_status_json(char *buf, int cap);
