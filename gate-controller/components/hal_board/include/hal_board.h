/*
 * hal_board.h — single source of truth for pin mapping (plan p.11.1).
 *
 * Stage 1: ESP32-S3-DevKitC-1 (WROOM-1-N16R8) on a breadboard.
 * Stage 2: custom PCB -> only the numbers below change, logic is untouched.
 *
 * DevKit constraints honored:
 *  - GPIO 0/45/46 are strapping pins -> not used as outputs (0 kept for BOOT only).
 *  - GPIO 19/20 = USB-Serial-JTAG D+/D- -> reserved for console while debugging.
 *  - GPIO 26-32 = SPI flash/PSRAM -> never usable.
 *  - ADC1 only (ADC2 conflicts with Wi-Fi): current sense + battery on ADC1 ch.
 *  - DVP camera needs ~12 GPIOs; on devkit this coexists with PCNT by using
 *    the "DI encoder via GPIO ISR" fallback (ENC_USE_GPIO_ISR, plan p.11.1).
 */
#pragma once
#include "esp_err.h"

/* ---- Power stage: triac (phase-angle) + direction relays ------------------ */
#define PIN_TRIAC_PWM          10   /* MCPWM0A -> gate via opto-triac driver      */
#define PIN_RELAY_OPEN         11   /* K1: forward half-wave path                 */
#define PIN_RELAY_CLOSE        12   /* K2: reverse half-wave path                 */
#define PIN_RELAY_SHUNT        13   /* K3: bypass triac at full speed (heat save) */
#define PIN_MAINS_DETECT       14   /* zero-cross + loss-of-mains comparator out  */

/* ---- Sensing ---------------------------------------------------------------*/
#define PIN_ENC_A              15   /* PCNT0 signal0 (or GPIO-ISR fallback)       */
#define PIN_ENC_B              16   /* PCNT0 signal1                              */
#define PIN_LIMIT_CLOSED       17   /* NC contact, active-low                     */
#define PIN_LIMIT_OPEN         18   /* NC contact, active-low                     */
#define PIN_PROBE_OBSTACLE     21   /* optional mechanical strip                  */

/* ---- UI --------------------------------------------------------------------*/
#define PIN_BTN_OPEN           4    /* RTC-capable (ext1 wake)                    */
#define PIN_BTN_STOP           5    /* RTC-capable                                */
#define PIN_BTN_CLOSE          6    /* RTC-capable                                */
#define PIN_BEEP               7    /* LEDC PWM buzzer                            */
#define PIN_STATUS_LED         38   /* WS2812 data or plain LED (see CFG)         */

/* ---- Camera DVP (OV5640, no IR-cut removed yet; ir_illum optional slot) --- */
#define PIN_CAM_PWDN           -1
#define PIN_CAM_XCLK           3
#define PIN_CAM_SIOD           8    /* SCCB/I2C SDA (shared bus w/ RTC if added)  */
#define PIN_CAM_SIOC           9    /* SCCB/I2C SCL                               */
#define PIN_CAM_D0             48
#define PIN_CAM_D1             47
#define PIN_CAM_D2             21   /* NOTE: collides w/ probe on devkit -> see    */
#define PIN_CAM_D3             14   /* README_devkit.md for the documented swap     */
#define PIN_CAM_D4             16
#define PIN_CAM_D5             15
#define PIN_CAM_D6             13
#define PIN_CAM_D7             12
#define PIN_CAM_VSYNC          10
#define PIN_CAM_HREF           11
#define PIN_CAM_PCLK           9
#define PIN_IR_ILLUM           39   /* 850nm LED driver EN (plan p.12.2 option)   */

/* ---- Analog ----------------------------------------------------------------*/
#define CH_CURRENT_CT          ADC1_CHANNEL_0  /* GPIO1: CT clamp RMS              */
#define CH_BATTERY             ADC1_CHANNEL_2  /* GPIO3: divider from batt pack    */
#define CH_TEMP_BATT           ADC1_CHANNEL_3  /* GPIO4: NTC for thermal interlock */

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
