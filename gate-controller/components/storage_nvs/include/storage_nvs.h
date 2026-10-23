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

/* Regional mains frequency for the triac phase-angle firing loop.
 * RU/EU/CIS default 50 Hz; US/JP/SA 60 Hz. AUTO = measure zero-cross period
 * at startup and latch the detected value (replaces old AC_PERIOD magic). */
typedef enum {
    MAINS_FREQ_AUTO = 0,
    MAINS_FREQ_50HZ = 50,
    MAINS_FREQ_60HZ = 60,
} mains_freq_sel_t;

/* Current-sensor variant fitted on the board (custom PCB carries both
 * footprints — easyeda/gate_v1_netlist.json; exactly one is populated):
 *   0 HALL : ACS712/ACS723 class + RRIO mixer (ratiometric, DC-capable)
 *   1 ZMCT : ZMCT103C split-core CT + bipolar burden on buffered mid-rail
 *            (AC only; battery telemetry then comes from VBAT divider only)
 * K/G semantics: see current_sense_set_front_end(). */
typedef uint8_t ct_sensor_sel_t;
#define CT_SEL_HALL 0u
#define CT_SEL_ZMCT 1u

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
    mains_freq_sel_t mains_freq; /* regional 50/60 Hz (or AUTO detect)      */
    uint8_t  region_code;        /* informational only: 0=other 1=RU/EU-50
                                    2=US/JP-60 — drives defaults/UI hints,
                                    never changes control behavior directly */
    ct_sensor_sel_t ct_sensor;   /* fitted current sensor variant           */
    uint16_t ct_k_mv_per_a;      /* transfer fn mV/A: HALL datasheet K,
                                    ZMCT burden slope Rb/N (0 -> built-in)   */
    uint16_t ct_vref_mv;         /* analog supply (HALL) / buffered VREF
                                  * (ZMCT); 0 -> 3300                        */
    uint16_t ct_i_full_a;        /* full-scale current for G derivation     */
    bool     pinch_detect_en;
    bool     ir_illum_policy_off;/* keep IR disabled until night decision   */
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
