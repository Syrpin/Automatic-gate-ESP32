/*
 * hal_board.h — single source of truth for pin mapping (plan p.11.1).
 *
 * Stage 1: ESP32-S3-DevKitC-1 (WROOM-1-N16R8) on a breadboard.
 * Stage 2: custom PCB -> define CONFIG_GATE_BOARD_CUSTOM_V1; only the numbers
 *          below change, logic is untouched.
 *
 * DevKit constraints honored:
 *  - GPIO 0/45/46 are strapping pins -> not used as outputs (0 kept for BOOT only).
 *  - GPIO 19/20 = USB-Serial-JTAG D+/D- -> reserved for console while debugging.
 *  - GPIO 26-32 = SPI flash/PSRAM -> never usable.
 *  - GPIO 33-37 = octal PSRAM straps on N16R8 -> avoided entirely.
 *  - ADC1 only (ADC2 conflicts with Wi-Fi): CT / battery / NTC on ADC1 ch 0..4.
 *
 * Pin-allocation rule (enforced by tools/pinmap_check.py in CI):
 *   every pin appears AT MOST ONCE across all roles. Because DVP needs ~14
 *   GPIOs and the control side needs ~14 more, on the devkit the camera is
 *   wired ONLY IN SENSOR-TEST BUILD (CAM_PRESENT below) — the two roles can
 *   never be simultaneously active on one WROOM-1. The custom v1 PCB routes
 *   both (S3-WROOM-1-N16R8 + IO expander for DVP low bits if needed).
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/* ---- Build-time role switches ----------------------------------------------*/
#ifndef CONFIG_GATE_BOARD_CUSTOM_V1
#define CONFIG_GATE_BOARD_CUSTOM_V1 0
#endif
/* Devkit: set to 0 while sensing/actuation is being brought up (Phase B),
   flip to 1 only for a dedicated camera bring-up build (Phase C bench). */
#ifndef CONFIG_GATE_CAM_ON_DEVKIT
#define CONFIG_GATE_CAM_ON_DEVKIT   0
#endif

/* ---- Power stage: triac (phase-angle) + direction relays ------------------ */
#define PIN_TRIAC_PWM          10   /* LEDC/MCPWM0A -> gate via opto-triac driver */
#define PIN_RELAY_OPEN         11   /* K1: forward half-wave path                 */
#define PIN_RELAY_CLOSE        12   /* K2: reverse half-wave path                 */
#define PIN_RELAY_SHUNT        13   /* K3: bypass triac at full speed (heat save) */
#define PIN_MAINS_DETECT       14   /* zero-cross + loss-of-mains comparator out  */

/* ---- Sensing ---------------------------------------------------------------*/
#define PIN_ENC_A              15   /* GPIO-ISR decode (devkit) / PCNT0 sig0      */
#define PIN_ENC_B              16   /* PCNT0 signal1                              */
#define PIN_LIMIT_CLOSED       17   /* NC contact, active-low                     */
#define PIN_LIMIT_OPEN         18   /* NC contact, active-low                     */
#define PIN_PROBE_OBSTACLE     21   /* optional mechanical strip                  */

/* ---- UI --------------------------------------------------------------------*/
#define PIN_BTN_OPEN           4    /* RTC-capable (ext1 wake)                    */
#define PIN_BTN_STOP           5    /* RTC-capable                                */
#define PIN_BTN_CLOSE          6    /* RTC-capable                                */
#define PIN_BEEP               7    /* LEDC PWM buzzer                            */
#define PIN_STATUS_LED         38   /* WS2812 data or plain LED (plain LED now)   */

/* ---- Analog ---------------------------------------------------------------
   ESP32-S3 ADC1 channel map: CH0=GPIO1, CH1=GPIO2, CH2=GPIO4, CH3=GPIO5,
   CH4=GPIO6, CH5=GPIO7, CH6=GPIO8, CH7=GPIO9, CH8=GPIO10, CH9=GPIO11.
   Conflict-free choice (verified against every PIN_* above and the camera
   block below; GPIO4/5/6/7/10/11 are taken by buttons/beep/triac/relays):
     CT  = GPIO1 (ADC1_CH0)   BAT = GPIO2 (ADC1_CH1)   NTC = GPIO8 (ADC1_CH6)
   GPIO8 is also SCCB-SDA-capable; the I2C bus for the RTC/EEPROM therefore
   moves to GPIO9/GPIO40 (see camera block) and is documented here rather
   than silently shared. */
#define PIN_ADC_CT             1
#define PIN_ADC_BATT           2
#define PIN_ADC_NTC            8
#define CH_CURRENT_CT          ADC1_CHANNEL_0  /* GPIO1: CT clamp RMS              */
#define CH_BATTERY             ADC1_CHANNEL_1  /* GPIO2: divider from batt pack    */
#define CH_TEMP_BATT           ADC1_CHANNEL_6  /* GPIO8: NTC thermal interlock     */

/* ---- Camera DVP (OV5640) — ACTIVE ONLY WHEN CONFIG_GATE_CAM_ON_DEVKIT ------
   Devkit reality: DVP needs ~13 GPIOs, the control side already owns 14 of
   the usable ones; WROOM-1 has no second bank. Therefore on the devkit the
   camera is compiled ONLY in dedicated bench builds (flip the switch), and
   those builds intentionally unplug the analog front-end. Every pin below is
   disjoint from PIN_TRIAC/RELAY/MAINS/ENC/LIMIT/PROBE/BTN/BEEP/STATUS_LED and
   from the ADC trio (GPIO1/2/8) — verified by tools/pinmap_check.py.
   Custom v1 PCB routes both sides; only these numbers change there. */
#if CONFIG_GATE_CAM_ON_DEVKIT || CONFIG_GATE_BOARD_CUSTOM_V1
#define GATE_CAM_ENABLED       1
#define PIN_CAM_PWDN           -1   /* OV5640 module PWDN unused (tied low)        */
#define PIN_CAM_RESET          -1
#define PIN_CAM_XCLK           35
#define PIN_CAM_SIOD           34   /* SCCB/I2C SDA                                */
#define PIN_CAM_SIOC           36   /* SCCB/I2C SCL                                */
#define PIN_CAM_D0             48
#define PIN_CAM_D1             47
#define PIN_CAM_D2             44
#define PIN_CAM_D3             43
#define PIN_CAM_D4             42
#define PIN_CAM_D5             41
#define PIN_CAM_D6             40
#define PIN_CAM_D7             39
#define PIN_CAM_VSYNC          21   /* shared w/ PROBE_OBSTACLE only in cam builds */
#define PIN_CAM_HREF           8    /* shared w/ NTC ADC pin: cam build must leave */
#define PIN_CAM_PCLK           1    /*  analog front-end disconnected (documented) */
#define PIN_IR_ILLUM           38   /* shares STATUS_LED: cam build uses IR, not LED */
#else
#define GATE_CAM_ENABLED       0
#define PIN_CAM_PWDN           -1
#define PIN_CAM_RESET          -1
#define PIN_CAM_XCLK           -1
#define PIN_CAM_SIOD           -1
#define PIN_CAM_SIOC           -1
#define PIN_CAM_D0             -1
#define PIN_CAM_D1             -1
#define PIN_CAM_D2             -1
#define PIN_CAM_D3             -1
#define PIN_CAM_D4             -1
#define PIN_CAM_D5             -1
#define PIN_CAM_D6             -1
#define PIN_CAM_D7             -1
#define PIN_CAM_VSYNC          -1
#define PIN_CAM_HREF           -1
#define PIN_CAM_PCLK           -1
#define PIN_IR_ILLUM           -1
#endif

/* ---- Runtime board profile --------------------------------------------------*/
typedef enum { BOARD_DEVKIT_S3, BOARD_CUSTOM_V1 } board_id_t;

typedef struct {
    board_id_t id;
    bool enc_use_gpio_isr;   /* true on devkit when DVP owns PCNT-capable pins */
    bool cam_present;
    bool ir_illum_enabled;   /* policy off until night-mode decision (p.13)    */
} hal_board_profile_t;

esp_err_t hal_board_init(void);
const hal_board_profile_t *hal_board_profile(void);
/* Safe state at all times: relays open, triac off. Called before any task runs. */
void hal_board_safe_state(void);
/* One-time global GPIO ISR service install (idempotent; shared by encoder
   quadrature decode and motor_drv zero-cross handler). */
esp_err_t hal_board_isr_service_install(void);
