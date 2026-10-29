#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef int gpio_num_t;
#define GPIO_NUM_NC (-1)
#define GPIO_NUM_0 0
#define GPIO_SEL(pin) (1ULL << (pin))
#define GPIO_MODE_DEF_INPUT 1
#define GPIO_MODE_DEF_OUTPUT 2

typedef enum {
    GPIO_MODE_DISABLE = 0,
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_OUTPUT_OD,
    GPIO_MODE_INPUT_OUTPUT_OD,
    GPIO_MODE_INPUT_OUTPUT,
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE_ONLY = 0,
    GPIO_PULL_UP_ONLY,
    GPIO_PULL_DOWN_ONLY,
    GPIO_PULL_UP_DOWN,
} gpio_pull_mode_t;

typedef enum {
    GPIO_INTR_DISABLE = 0,
    GPIO_INTR_POSEDGE,
    GPIO_INTR_NEGEDGE,
    GPIO_INTR_ANYEDGE,
} gpio_int_type_t;

typedef enum { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE } gpio_pullup_t;
typedef enum { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE } gpio_pulldown_t;

typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

typedef void (*gpio_isr_handler_t)(void *arg);

static inline esp_err_t gpio_config(const gpio_config_t *c) { (void)c; return ESP_OK; }
static inline esp_err_t gpio_set_level(gpio_num_t p, uint32_t l) { (void)p; (void)l; return ESP_OK; }
static inline int gpio_get_level(gpio_num_t p) { (void)p; return 0; }
static inline esp_err_t gpio_set_direction(gpio_num_t p, gpio_mode_t m) { (void)p; (void)m; return ESP_OK; }
static inline esp_err_t gpio_set_pull_mode(gpio_num_t p, gpio_pull_mode_t m) { (void)p; (void)m; return ESP_OK; }
static inline esp_err_t gpio_install_isr_service(int flags) { (void)flags; return ESP_OK; }
static inline esp_err_t gpio_remove_isr_service(void) { return ESP_OK; }
static inline esp_err_t gpio_isr_handler_add(gpio_num_t p, gpio_isr_handler_t h, void *arg) {
    (void)p; (void)h; (void)arg; return ESP_OK;
}
static inline esp_err_t gpio_isr_handler_remove(gpio_num_t p) { (void)p; return ESP_OK; }
static inline esp_err_t gpio_intr_disable(gpio_num_t p) { (void)p; return ESP_OK; }
static inline esp_err_t gpio_intr_enable(gpio_num_t p) { (void)p; return ESP_OK; }
#define ESP_INTR_FLAG_DEFAULT 0
#define ESP_INTR_FLAG_LEVEL1 1
#define ESP_INTR_FLAG_IRAM 4
