/* hal_board.c — board bring-up, safe state, profile selection. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal_board.h"

static const char *TAG = "hal";

static hal_board_profile_t s_profile = {
    .id = BOARD_DEVKIT_S3,
    /* Devkit: quadrature decode via GPIO ISR (PCNT left free for future use).
       Custom PCB v1 switches this to PCNT by hardware design. */
    .enc_use_gpio_isr = true,
    .cam_present = (GATE_CAM_ENABLED != 0),
    .ir_illum_enabled = false,
};

void hal_board_safe_state(void)
{
    gpio_set_level(PIN_TRIAC_PWM, 0);
    gpio_set_level(PIN_RELAY_OPEN, 0);
    gpio_set_level(PIN_RELAY_CLOSE, 0);
    gpio_set_level(PIN_RELAY_SHUNT, 0);
    gpio_set_level(PIN_BEEP, 0);
#if GATE_CAM_ENABLED
    gpio_set_level(PIN_IR_ILLUM, 0);
#endif
}

esp_err_t hal_board_isr_service_install(void)
{
    /* Installed ONCE for the whole system; encoder (quadrature) and motor_drv
       (zero-cross) attach their handlers to the same service. ESP_ERR_INVALID_STATE
       means somebody already installed it -> not an error for us. */
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    return (err == ESP_ERR_INVALID_STATE) ? ESP_OK : err;
}

esp_err_t hal_board_init(void)
{
    /* Outputs first, with safe levels BEFORE direction is enabled:
       mirrors the old gpio.c but without any blocking wait loops.
       -1 pins (camera in sensing build) are skipped by gpio_reset_pin guard. */
    const int outs[] = { PIN_TRIAC_PWM, PIN_RELAY_OPEN, PIN_RELAY_CLOSE,
                         PIN_RELAY_SHUNT, PIN_BEEP, PIN_STATUS_LED };
    for (size_t i = 0; i < sizeof outs / sizeof outs[0]; i++) {
        if (outs[i] < 0) continue;
        gpio_reset_pin(outs[i]);
        gpio_set_direction(outs[i], GPIO_MODE_OUTPUT);
        gpio_set_level(outs[i], 0);
    }
#if GATE_CAM_ENABLED
    if (PIN_IR_ILLUM >= 0) {
        gpio_reset_pin(PIN_IR_ILLUM);
        gpio_set_direction(PIN_IR_ILLUM, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_IR_ILLUM, 0);   /* IR policy: off until night decision */
    }
#endif

    const int ins[] = { PIN_MAINS_DETECT, PIN_LIMIT_CLOSED, PIN_LIMIT_OPEN,
                        PIN_PROBE_OBSTACLE, PIN_BTN_OPEN, PIN_BTN_STOP,
                        PIN_BTN_CLOSE };
    for (size_t i = 0; i < sizeof ins / sizeof ins[0]; i++) {
        if (ins[i] < 0) continue;
        gpio_reset_pin(ins[i]);
        gpio_set_direction(ins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(ins[i], GPIO_PULL_UP_ONLY); /* NC contacts idle-high */
    }

    /* ADC pins are configured by the adc_oneshot driver, never as GPIO. */
    hal_board_safe_state();
    ESP_LOGI(TAG, "board=%s enc=%s cam=%d",
             s_profile.id == BOARD_DEVKIT_S3 ? "devkit-s3" : "custom-v1",
             s_profile.enc_use_gpio_isr ? "gpio-ISR" : "PCNT",
             s_profile.cam_present);
    return ESP_OK;
}

const hal_board_profile_t *hal_board_profile(void) { return &s_profile; }
