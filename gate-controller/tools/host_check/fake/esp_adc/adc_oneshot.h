#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef enum { ADC_UNIT_1 = 1, ADC_UNIT_2 } adc_unit_t;
typedef enum { ADC_ATTEN_0DB = 0, ADC_ATTEN_DB_2_5 = 1, ADC_ATTEN_DB_6 = 2, ADC_ATTEN_DB_12 = 3 } adc_atten_t;
#define ADC_ATTEN_DB_2_5_LEGACY ADC_ATTEN_DB_2_5
typedef int adc_channel_t;
#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define ADC_CHANNEL_2 2
#define ADC_CHANNEL_3 3
#define ADC_CHANNEL_4 4
#define ADC_CHANNEL_5 5
#define ADC_CHANNEL_6 6
#define ADC_CHANNEL_7 7
#define ADC_NUM_MAX 8

typedef enum { ADC_WIDTH_BIT_12 = 12, ADC_WIDTH_BIT_DEFAULT = 12 } adc_bits_width_t;
#define ADC1_CHANNEL_0 0
#define ADC1_CHANNEL_1 1
#define ADC1_CHANNEL_2 2
#define ADC1_CHANNEL_3 3
#define ADC1_CHANNEL_4 4
#define ADC1_CHANNEL_5 5
#define ADC1_CHANNEL_6 6
#define ADC1_CHANNEL_7 7

typedef void *adc_oneshot_unit_handle_t;
typedef struct { adc_unit_t unit_id; uint32_t clk_src; } adc_oneshot_unit_init_cfg_t;
typedef struct { adc_atten_t atten; adc_bits_width_t bitwidth; } adc_oneshot_chan_cfg_t;
static inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *c, adc_oneshot_unit_handle_t *h) { (void)c; if (h) *h = (void*)1; return ESP_OK; }
static inline esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t h, int ch, const adc_oneshot_chan_cfg_t *c) { (void)h; (void)ch; (void)c; return ESP_OK; }
static inline esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t h, int ch, int *raw) { (void)h; (void)ch; if (raw) *raw = 0; return ESP_OK; }
static inline esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t h) { (void)h; return ESP_OK; }

/* legacy esp_adc/adc_continuous etc not needed */
