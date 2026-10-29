#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
#include <stdint.h>

typedef void *pcnt_unit_handle_t;
typedef void *pcnt_channel_handle_t;
static inline esp_err_t pcnt_new_unit(const void *c, pcnt_unit_handle_t *h) { (void)c; if (h) *h = (void*)1; return ESP_OK; }
static inline esp_err_t pcnt_new_channel(pcnt_unit_handle_t u, const void *c, pcnt_channel_handle_t *h) { (void)u; (void)c; if (h) *h = (void*)1; return ESP_OK; }
static inline esp_err_t pcnt_unit_add_watch_point(pcnt_unit_handle_t u, int w) { (void)u; (void)w; return ESP_OK; }
static inline esp_err_t pcnt_unit_enable(pcnt_unit_handle_t u) { (void)u; return ESP_OK; }
static inline esp_err_t pcnt_unit_start(pcnt_unit_handle_t u) { (void)u; return ESP_OK; }
static inline esp_err_t pcnt_unit_stop(pcnt_unit_handle_t u) { (void)u; return ESP_OK; }
static inline esp_err_t pcnt_unit_get_count(pcnt_unit_handle_t u, int *v) { (void)u; if (v) *v = 0; return ESP_OK; }
static inline esp_err_t pcnt_unit_clear_count(pcnt_unit_handle_t u) { (void)u; return ESP_OK; }
