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
#if defined(CONFIG_GATE_MAINS_50HZ)
    .mains_freq        = MAINS_FREQ_50HZ, .region_code = 1,
#elif defined(CONFIG_GATE_MAINS_60HZ)
    .mains_freq        = MAINS_FREQ_60HZ, .region_code = 2,
#else
    .mains_freq        = MAINS_FREQ_AUTO, /* Kconfig default: AUTO-detect   */
#endif
    /* DECISION 2026: ZMCT103C split-core CT is the PRIMARY sensor —
     * galvanic isolation, zero offset drift, AC-only is fine (pinch/RMS
     * protection is inherently mains-synced; battery telemetry dropped by
     * decision, so no DC-capable Hall channel is needed). The Hall front
     * end (ACS712/ACS723 class) remains selectable as legacy bring-up
     * fallback (devkit / variant-A board). Kconfig GATE_CT_* only changes
     * this first-boot default; runtime switch: NVS / web UI / "ct" console. */
    .ct_sensor         = CT_SEL_ZMCT, .ct_k_mv_per_a = 22,
    .ct_vref_mv        = 3300, .ct_i_full_a = 30,
    .pinch_detect_en   = true,
    .ir_illum_policy_off = true,
};

static bool validate(const gate_config_t *c)
{
    if (!(c->travel_counts >= 100 && c->enc_min >= 1 &&
          c->ramp_up_ms >= 200 && c->duty_max_pct <= 100 &&
          c->i_rms_run_max > 0.1f && c->i_rms_peak_max > c->i_rms_run_max))
        return false;
    /* regional sanity: explicit selection must be 50 or 60 (or AUTO);
       region_code is informational but must not contradict an explicit pick */
    if (c->mains_freq != MAINS_FREQ_AUTO &&
        c->mains_freq != MAINS_FREQ_50HZ &&
        c->mains_freq != MAINS_FREQ_60HZ)
        return false;
    if (c->region_code == 1 && c->mains_freq == MAINS_FREQ_60HZ) return false; /* EU w/ 60Hz */
    if (c->region_code == 2 && c->mains_freq == MAINS_FREQ_50HZ) return false; /* US w/ 50Hz */
    /* CT variant: only HALL/ZMCT known; per-variant plausible K range guards
     * against swapped values (Hall 30A=66 mV/A vs ZMCT burden 22 mV/A). */
    if (c->ct_sensor != CT_SEL_HALL && c->ct_sensor != CT_SEL_ZMCT) return false;
    if (c->ct_k_mv_per_a) {
        uint16_t lo = (c->ct_sensor == CT_SEL_ZMCT) ? 5 : 20;
        uint16_t hi = (c->ct_sensor == CT_SEL_ZMCT) ? 200 : 400;
        if (c->ct_k_mv_per_a < lo || c->ct_k_mv_per_a > hi) return false;
    }
    if (c->ct_i_full_a && c->ct_i_full_a > 100) return false;
    return true;
}

/* Layout migration: configs committed before the ct_* fields existed
 * deserialize with a blob smaller than the struct. Keep the stored prefix,
 * backfill the new tail from compile-time defaults, and re-save — so an old
 * NVS image never silently runs with bogus K/G on a ZMCT board. */
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
        gate_config_t tmp = s_cfg;              /* compile-time defaults   */
        size_t sz = sizeof tmp;
        bool migrated = false;
        if (nvs_get_blob(h, "gate", &tmp, &sz) == ESP_OK) {
            if (sz < sizeof tmp) {              /* legacy blob -> backfill  */
                const char *def = (const char*)&s_cfg;
                char *dst = (char*)&tmp;
                for (size_t i = sz; i < sizeof tmp; i++) dst[i] = def[i];
                migrated = true;
            }
            if (validate(&tmp)) s_cfg = tmp;
            else ESP_LOGE(TAG, "stored config invalid -> defaults");
        }
        nvs_close(h);
        if (migrated) {
            ESP_LOGI(TAG, "config layout upgraded (ct_* fields), re-saving");
            storage_cfg_commit(&s_cfg);
        }
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
