#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>

typedef enum { LEDC_AUTO_CLK = 0, LEDC_USE_RTCCLK = 1, LEDC_USE_APB_CLK = 2 } ledc_clk_cfg_t;
typedef enum { LEDC_HIGH_SPEED_MODE = 0, LEDC_LOW_SPEED_MODE } ledc_mode_t;
typedef enum { LEDC_TIMER_0 = 0, LEDC_TIMER_1, LEDC_TIMER_2, LEDC_TIMER_3 } ledc_timer_t;
typedef enum {
    LEDC_CHANNEL_0 = 0, LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3,
    LEDC_CHANNEL_4, LEDC_CHANNEL_5, LEDC_CHANNEL_6, LEDC_CHANNEL_7 } ledc_channel_t;
typedef enum {
    LEDC_TIMER_1_BIT = 1, LEDC_TIMER_8_BIT = 8, LEDC_TIMER_10_BIT = 10,
    LEDC_TIMER_12_BIT = 12, LEDC_TIMER_13_BIT = 13, LEDC_TIMER_14_BIT = 14 } ledc_timer_bit_t;

typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_bit_t duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
} ledc_timer_config_t;

typedef struct {
    ledc_mode_t speed_mode;
    int32_t duty;
    ledc_channel_t channel;
    gpio_num_t gpio_num;
    ledc_timer_t timer_num;
    uint32_t hpoint;
    bool intr_enable;
    ledc_timer_t timer_sel;
} ledc_channel_config_t;

static inline esp_err_t ledc_timer_config(const ledc_timer_config_t *c) { (void)c; return ESP_OK; }
static inline esp_err_t ledc_channel_config(const ledc_channel_config_t *c) { (void)c; return ESP_OK; }
static inline esp_err_t ledc_set_freq(ledc_mode_t m, ledc_timer_t t, uint32_t f) { (void)m; (void)t; (void)f; return ESP_OK; }
static inline esp_err_t ledc_set_duty(ledc_mode_t m, ledc_channel_t ch, uint32_t d) { (void)m; (void)ch; (void)d; return ESP_OK; }
static inline uint32_t ledc_get_duty(ledc_mode_t m, ledc_channel_t ch) { (void)m; (void)ch; return 0; }
static inline esp_err_t ledc_update_duty(ledc_mode_t m, ledc_channel_t ch) { (void)m; (void)ch; return ESP_OK; }
static inline esp_err_t ledc_stop(ledc_mode_t m, ledc_channel_t ch, uint32_t idle) { (void)m; (void)ch; (void)idle; return ESP_OK; }
#define LEDC_DUTY_MAX_13BIT (8191)
