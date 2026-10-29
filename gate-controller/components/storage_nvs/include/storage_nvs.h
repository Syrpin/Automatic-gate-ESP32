/*
 * storage_nvs.h — configuration + persisted gate state (NVS).
 * Secrets (Wi-Fi keys, Telegram token) live in NVS namespace "cfg" so that
 * enabling Flash Encryption later protects them without code changes (p.9.2).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"

typedef enum {
    ENC_TYPE_OPTICAL = 0,   /* A/B quadrature, hardware PCNT or GPIO ISR   */
    ENC_TYPE_DX            = 1,   /* Reed-disk: single channel per direction */
    ENC_TYPE_DI            = 2,   /* Inductive: pulse train, dir from limits */
} encoder_type_t;

typedef struct {
    /* mechanical calibration */
    int32_t  travel_counts;      /* closed->open in encoder counts          */
    int32_t  enc_min;            /* min valid counts/step (was float bug)   */
    uint16_t ramp_up_ms;         /* soft-start time                         */
    uint16_t ramp_down_ms;       /* deceleration before limits              */
    uint16_t duty_max_pct;       /* phase-angle ceiling for cold starts     */
    /* current protection */
    float    i_rms_run_max;      /* stall threshold, A                      */
    float    i_rms_peak_max;     /* inrush allowance during first N ms, A   */
    uint16_t inrush_window_ms;
    /* environment / profiles */
    encoder_type_t enc_type;
    bool     pinch_detect_en;
    bool     ir_illum_policy_off;/* keep IR disabled until night decision   */
    /* CT calibration (wizard, Phase E): amps per ADC-LSB of the centered
       window. 0 => "not calibrated": overcurrent/pinch/no-current protections
       stay INACTIVE by design (fail-safe: uncalibrated gain must never trip
       or mask protection silently — current_sense logs a warning once). */
    float    ct_gain_a_per_lsb;
} gate_config_t;

esp_err_t storage_nvs_init(void);
const gate_config_t *storage_cfg(void);
esp_err_t storage_cfg_commit(const gate_config_t *c);   /* validated write  */

/* Position persistence: called on every meaningful move-end and by power_mgmt
 * on loss-of-mains within the capacitor hold-up window (plan p.9.1). */
esp_err_t storage_save_position(int32_t pos_counts, uint32_t epoch, bool trusted);
esp_err_t storage_load_position(int32_t *pos_counts, uint32_t *epoch, bool *trusted);

/* Pending-notification flag survives reboot if router was also dark (p.9.1). */
esp_err_t storage_set_pending_notify(bool pending);
bool      storage_get_pending_notify(void);
