#pragma once
#include "esp_err.h"
#define ESP_RETURN_ON_ERROR(x, tag, fmt, ...) \
    do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) { return err_rc_; } } while (0)
#define ESP_GOTO_ON_ERROR(x, lbl, tag, fmt, ...) \
    do { esp_err_t err_rc_ = (x); if (err_rc_ != ESP_OK) { goto lbl; } } while (0)
#define ESP_RETURN_ON_FALSE(a, err_code, tag, fmt, ...) \
    do { if (!(a)) { return err_code; } } while (0)
