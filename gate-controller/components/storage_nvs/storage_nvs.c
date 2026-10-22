/* storage_nvs.c — NVS-backed config & position with validation. */
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_check.h"
#include "storage_nvs.h"

static const char *TAG = "nvs";
#define NS "cfg"

static gate_config_t s_cfg = {
    /* Conservative defaults; wizard (Phase E) overwrites via calibration. */
    .travel_counts     = 4000,
    .enc_min           = 2,
    .ramp_up_ms        = 1500,
    .ramp_down_ms      = 800,
    .duty_max_pct      = 90,
    .i_rms_run_max     = 6.0f,
    .i_rms_peak_max    = 14.0f,
    .inrush_window_ms  = 700,
    .enc_type          = ENC_TYPE_OPTICAL,
    .pinch_detect_en   = true,
    .ir_illum_policy_off = true,
};

static bool validate(const gate_config_t *c)
{
    return c->travel_counts >= 100 && c->enc_min >= 1 &&
           c->ramp_up_ms >= 200 && c->duty_max_pct <= 100 &&
           c->i_rms_run_max > 0.1f && c->i_rms_peak_max > c->i_rms_run_max;
}

esp_err_t storage_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "erasing NVS (first boot / version change)");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof s_cfg;
        if (nvs_get_blob(h, "gate", &s_cfg, &sz) == ESP_OK && !validate(&s_cfg)) {
            ESP_LOGE(TAG, "stored config invalid -> defaults");
        }
        nvs_close(h);
    }
    return ESP_OK;
}

const gate_config_t *storage_cfg(void) { return &s_cfg; }

esp_err_t storage_cfg_commit(const gate_config_t *c)
{
    if (!validate(c)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NS, NVS_READWRITE, &h), TAG, "open");
    esp_err_t err = nvs_set_blob(h, "gate", c, sizeof *c);
    if (!err) err = nvs_commit(h);
    nvs_close(h);
    if (!err) s_cfg = *c;
    return err;
}

esp_err_t storage_save_position(int32_t pos, uint32_t epoch, bool trusted)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NS, NVS_READWRITE, &h), TAG, "open");
    esp_err_t e1 = nvs_set_i32(h, "pos", pos);
    esp_err_t e2 = nvs_set_u32(h, "epoch", epoch);
    esp_err_t e3 = nvs_set_u8(h, "trusted", trusted ? 1 : 0);
    esp_err_t ec = nvs_commit(h);
    nvs_close(h);
    return e1 ? e1 : (e2 ? e2 : (e3 ? e3 : ec));
}

esp_err_t storage_load_position(int32_t *pos, uint32_t *epoch, bool *trusted)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NS, NVS_READONLY, &h), TAG, "open");
    esp_err_t err = nvs_get_i32(h, "pos", pos);
    if (!err) err = nvs_get_u32(h, "epoch", epoch);
    uint8_t t = 0;
    if (!err) err = nvs_get_u8(h, "trusted", &t);
    *trusted = (t == 1);
    nvs_close(h);
    return err;
}

esp_err_t storage_set_pending_notify(bool pending)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(NS, NVS_READWRITE, &h), TAG, "open");
    esp_err_t err = nvs_set_u8(h, "pend_ntf", pending ? 1 : 0);
    if (!err) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool storage_get_pending_notify(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t v = 0;
    nvs_get_u8(h, "pend_ntf", &v);
    nvs_close(h);
    return v == 1;
}
