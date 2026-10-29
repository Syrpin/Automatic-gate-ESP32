#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
} esp_task_wdt_config_t;

#define ESP_TASK_WDT_CONFIG_DEFAULT { .timeout_ms = 5000, .idle_core_mask = 3, .trigger_panic = true }
#define WDT_STAGE_ID_NONE 0

static inline esp_err_t _twdt_dispatch_cfg(const esp_task_wdt_config_t *c) { (void)c; return ESP_OK; }
static inline esp_err_t _twdt_dispatch_u32(uint32_t t, bool p) { (void)t; (void)p; return ESP_OK; }
#define _TWD_GET(_1,_2,NAME,...) NAME
#define _twdt_dispatch(...) _TWD_GET(__VA_ARGS__, _twdt_dispatch_u32, _twdt_dispatch_cfg)(__VA_ARGS__)
#define esp_task_wdt_init(...) _twdt_dispatch(__VA_ARGS__)
static inline esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *c) { (void)c; return ESP_OK; }
static inline esp_err_t esp_task_wdt_add(TaskHandle_t t) { (void)t; return ESP_OK; }
static inline esp_err_t esp_task_wdt_reset(void) { return ESP_OK; }
static inline esp_err_t esp_task_wdt_delete(TaskHandle_t t) { (void)t; return ESP_OK; }
static inline void esp_task_wdt_dump(void *out) { (void)out; }
