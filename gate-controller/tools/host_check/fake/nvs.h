#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

typedef uint32_t nvs_handle_t;
typedef nvs_handle_t nvs_handle;
#define NVS_READONLY 0
#define NVS_READWRITE 1

typedef enum {
    NVS_TYPE_ANY = 0xff,
    NVS_TYPE_U8 = 1,
    NVS_TYPE_I8,
    NVS_TYPE_U16,
    NVS_TYPE_I16,
    NVS_TYPE_U32,
    NVS_TYPE_I32,
    NVS_TYPE_U64,
    NVS_TYPE_I64,
    NVS_TYPE_STR,
    NVS_TYPE_BLOB,
} nvs_type_t;

static inline esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h) { (void)ns; (void)mode; if (h) *h = 1; return ESP_OK; }
static inline esp_err_t nvs_open_ext(const char *ns, int mode, const void *cfg, nvs_handle_t *h) { (void)cfg; return nvs_open(ns, mode, h); }
static inline void nvs_close(nvs_handle_t h) { (void)h; }
static inline esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
static inline esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_i8(nvs_handle_t h, const char *k, int8_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_u16(nvs_handle_t h, const char *k, uint16_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_u32(nvs_handle_t h, const char *k, uint32_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_i32(nvs_handle_t h, const char *k, int32_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_u64(nvs_handle_t h, const char *k, uint64_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_i64(nvs_handle_t h, const char *k, int64_t *v) { (void)h; (void)k; (void)v; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *s, size_t *len) { (void)h; (void)k; (void)s; (void)len; return ESP_ERR_NVS_NOT_FOUND; }
static inline esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v) { (void)h; (void)k; (void)v; return ESP_OK; }
static inline esp_err_t nvs_set_u32(nvs_handle_t h, const char *k, uint32_t v) { (void)h; (void)k; (void)v; return ESP_OK; }
static inline esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v) { (void)h; (void)k; (void)v; return ESP_OK; }
static inline esp_err_t nvs_set_u64(nvs_handle_t h, const char *k, uint64_t v) { (void)h; (void)k; (void)v; return ESP_OK; }
static inline esp_err_t nvs_set_i64(nvs_handle_t h, const char *k, int64_t v) { (void)h; (void)k; (void)v; return ESP_OK; }
static inline esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *s) { (void)h; (void)k; (void)s; return ESP_OK; }
static inline esp_err_t nvs_erase_key(nvs_handle_t h, const char *k) { (void)h; (void)k; return ESP_OK; }
static inline esp_err_t nvs_erase_all(nvs_handle_t h) { (void)h; return ESP_OK; }
