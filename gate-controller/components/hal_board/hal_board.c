/* hal_board.c — board bring-up, safe state, profile selection. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "hal_board.h"

static const char *TAG = "hal";
#include "esp_log.h"

static hal_board_profile_t s_profile = {
    .id = BOARD_DEVKIT_S3,
    /* On devkit DVP owns most pins; encoder falls back to GPIO ISR (p.11.1).
       Custom PCB v1 switches this to PCNT by hardware design. */
    .enc_use_gpio_isr = true,
    .cam_present = true,
    .ir_illum_enabled = false,
};

void hal_board_safe_state(void)
{
    gpio_set_level(PIN_TRIAC_PWM, 0);
    gpio_set_level(PIN_RELAY_OPEN, 0);
    gpio_set_level(PIN_RELAY_CLOSE, 0);
    gpio_set_level(PIN_RELAY_SHUNT, 0);
    gpio_set_level(PIN_BEEP, 0);
}

esp_err_t hal_board_init(void)
{
    /* Outputs first, with safe levels BEFORE direction is enabled:
       mirrors the old gpio.c but without any blocking wait loops. */
    const int outs[] = { PIN_TRIAC_PWM, PIN_RELAY_OPEN, PIN_RELAY_CLOSE,
                         PIN_RELAY_SHUNT, PIN_BEEP, PIN_STATUS_LED, PIN_IR_ILLUM };
    for (size_t i = 0; i < sizeof outs / sizeof outs[0]; i++) {
        gpio_reset_pin(outs[i]);
        gpio_set_direction(outs[i], GPIO_MODE_OUTPUT);
        gpio_set_level(outs[i], 0);
    }

    const int ins[] = { PIN_MAINS_DETECT, PIN_LIMIT_CLOSED, PIN_LIMIT_OPEN,
                        PIN_PROBE_OBSTACLE, PIN_BTN_OPEN, PIN_BTN_STOP,
                        PIN_BTN_CLOSE };
    for (size_t i = 0; i < sizeof ins / sizeof ins[0]; i++) {
        gpio_reset_pin(ins[i]);
        gpio_set_direction(ins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(ins[i], GPIO_PULLUP_ONLY); /* NC contacts idle-high */
    }

    hal_board_safe_state();
    ESP_LOGI(TAG, "board=%s enc=%s cam=%d",
             s_profile.id == BOARD_DEVKIT_S3 ? "devkit-s3" : "custom-v1",
             s_profile.enc_use_gpio_isr ? "gpio-ISR" : "PCNT",
             s_profile.cam_present);
    return ESP_OK;
}

const hal_board_profile_t *hal_board_profile(void) { return &s_profile; }
