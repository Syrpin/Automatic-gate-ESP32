/*
 * gate_logic.h — the heart: explicit state machine replacing old CONDITION
 * bit-mask. One owner of "what should the motor be doing", cross-checking
 * encoder/limits/current (triple validation) and emitting semantic events.
 */
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "common_types.h"
#include "motor_drv.h"

typedef enum {
    GS_IDLE_CLOSED, GS_IDLE_OPEN, GS_IDLE_MID,   /* trusted position classes  */
    GS_MOVING_OPEN, GS_MOVING_CLOSE,
    GS_PINCH_REVERSE,                            /* anti-pinch pause+reverse   */
    GS_CALIBRATING,                              /* wizard-driven slow search  */
    GS_FAULT,                                    /* latched until user reset   */
} gate_state_t;

typedef struct {
    int32_t position, target;
    gate_state_t state;
    float current_rma; float batt_v; bool mains_ok;
} telemetry_snapshot_t;

esp_err_t gate_logic_init(void);      /* restore pos + validate vs limits     */
void      gate_logic_step(const TickPeriod_t *tp);
void      gate_logic_beep_tick(uint32_t dt_ms);
void      gate_logic_snapshot(telemetry_snapshot_t *s);

/* external command sources (buttons now; MQTT/web/vision later) */
void gate_cmd_open(void); void gate_cmd_close(void); void gate_cmd_stop(void);
/* vision hook (plan p.12): INSIDE-aperture confirmation allows auto-close */
void gate_vision_event(int kind, int vehicle_id);
