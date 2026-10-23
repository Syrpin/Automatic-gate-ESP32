/* webui.c — esp-http-server + tiny embedded SPA (see assets/index.html). */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "nvs.h"

#include "webui.h"
#include "sim.h"
#include "gate_logic.h"
#include "motor_drv.h"
#include "events_log.h"
#include "storage_nvs.h"

static const char *TAG = "webui";
static httpd_handle_t s_server;

/* --- embedded via CMake EMBED_FILE_TXT (assets/index.html) --------- */
extern const char index_html[];
extern const unsigned int index_html_len;

/* ------------------------------------------------------------------ */
static esp_err_t set_cors(httpd_req_t *r)
{
    httpd_resp_set_hdr(r, "Access-Control-Allow-Origin", "*");
    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    set_cors(r);
    httpd_resp_send(r, index_html, index_html_len);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *r)
{
    char buf[1024];
    int n = sim_status_json(buf, sizeof buf);
    httpd_resp_set_type(r, "application/json");
    set_cors(r);
    httpd_resp_send(r, buf, n);
    return ESP_OK;
}

/* very small hand-rolled JSON field extractors (no cJSON dependency on the
   hot path; payloads here are flat and trusted-local) */
static bool json_str(const char *body, const char *key, char *out, size_t cap)
{
    char pat[32]; snprintf(pat, sizeof pat, "\"%s\":\"", key);
    const char *p = strstr(body, pat);
    if (!p) return false;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) out[i++] = *p++;
    out[i] = 0;
    return true;
}
static bool json_num(const char *body, const char *key, double *out)
{
    char pat[32]; snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(body, pat);
    if (!p) return false;
    *out = strtod(p + strlen(pat), NULL);
    return true;
}
static bool json_bool(const char *body, const char *key, bool *out)
{
    char pat[32]; snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(body, pat);
    if (!p) return false;
    p += strlen(pat);
    *out = (p[0] == 't' || p[0] == '1');
    return true;
}

static esp_err_t cmd_handler(httpd_req_t *r)
{
    char body[192]; int len = httpd_req_recv(r, body, sizeof body - 1);
    if (len <= 0) { httpd_resp_sendstr(r, "{\"err\":\"recv\"}"); return ESP_OK; }
    body[len] = 0;
    char cmd[24];
    if (!json_str(body, "cmd", cmd, sizeof cmd)) { httpd_resp_sendstr(r, "{\"err\":\"cmd?\"}"); return ESP_OK; }
    if      (!strcmp(cmd, "open"))  gate_cmd_open();
    else if (!strcmp(cmd, "close")) gate_cmd_close();
    else if (!strcmp(cmd, "stop"))  gate_cmd_stop();
    else { httpd_resp_sendstr(r, "{\"err\":\"bad cmd\"}"); return ESP_OK; }
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t sim_handler(httpd_req_t *r)
{
    char body[192]; int len = httpd_req_recv(r, body, sizeof body - 1);
    if (len <= 0) { httpd_resp_sendstr(r, "{\"err\":\"recv\"}"); return ESP_OK; }
    body[len] = 0;

    char b[16];
    if (json_str(body, "btn", b, sizeof b)) {
        int idx = !strcmp(b, "open") ? 0 : !strcmp(b, "stop") ? 1 : 2;
        double ms = 150; json_num(body, "ms", &ms);
        sim_btn_press(idx, (uint32_t)ms);
    }
    if (json_str(body, "limit", b, sizeof b)) {
        double v = 0; json_num(body, "engaged", &v);
        sim_limit_set(!strcmp(b, "closed") ? 0 : 1, v != 0);
    }
    double amps;
    if (json_num(body, "i", &amps)) sim_current_inject((float)amps);
    bool bv;
    if (json_bool(body, "mains", &bv)) sim_mains_set(bv);
    if (json_bool(body, "plant", &bv)) sim_plant_model_enable(bv);
    /* regional mains frequency: {"ac":50|60|0} sets selection,
       {"zc":50|60|-1} starts/stops the virtual zero-cross generator */
    double ac;
    if (json_num(body, "ac", &ac)) {
        /* full regional profile: freq + informational code committed together
         * through the SAME validated path as the console `region` command */
        gate_config_t c = *storage_cfg();
        c.mains_freq = (ac == 50) ? MAINS_FREQ_50HZ :
                       (ac == 60) ? MAINS_FREQ_60HZ : MAINS_FREQ_AUTO;
        c.region_code = (ac == 50) ? 1 : (ac == 60) ? 2 : 0;
        if (storage_cfg_commit(&c) == ESP_OK)
            motor_drv_set_mains_freq(c.mains_freq);
    }
    if (json_num(body, "zc", &ac)) {
        if (ac < 0) sim_zc_stop(); else sim_zc_start((uint16_t)ac);
    }
    httpd_resp_sendstr(r, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t log_handler(httpd_req_t *r)
{
    /* last N journal lines streamed from the RAM ring (Phase E: flash tail) */
    extern int events_log_tail(char *buf, int cap);
    char buf[2048];
    int n = events_log_tail(buf, sizeof buf);
    httpd_resp_set_type(r, "application/json");
    set_cors(r);
    httpd_resp_send(r, buf, n);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
static void wifi_ap_fallback(void)
{
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wc = { .ap = { .ssid = "gate-cfg", .max_connection = 4,
                                 .authmode = WIFI_AUTH_OPEN } };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "softAP up: connect to 'gate-cfg' -> http://192.168.4.1/");
}

static void wifi_sta_start(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wc = {};
    strlcpy((char *)wc.sta.ssid, ssid, sizeof wc.sta.ssid);
    strlcpy((char *)wc.sta.password, pass, sizeof wc.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static esp_err_t start_httpd(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.server_port = 80;
    c.uri_match_fn = httpd_uri_match_wildcard;   /* "/" serves index too */
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &c), TAG, "httpd start");

    const httpd_uri_t uris[] = {
        { .uri = "/",           .method = HTTP_GET,  .handler = root_handler },
        { .uri = "/api/status", .method = HTTP_GET,  .handler = status_handler },
        { .uri = "/api/cmd",    .method = HTTP_POST, .handler = cmd_handler },
        { .uri = "/api/sim",    .method = HTTP_POST, .handler = sim_handler },
        { .uri = "/api/log",    .method = HTTP_GET,  .handler = log_handler },
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++)
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &uris[i]), TAG, "uri %s", uris[i].uri);
    return ESP_OK;
}

esp_err_t webui_start(void)
{
    /* credentials may be overridden at runtime via NVS ("cfg"/"wifi_ssid").
       Falls back to sdkconfig strings, then to an open config AP so a bare
       board with no Wi-Fi creds is still reachable over HTTP. */
    char ssid[33] = CONFIG_WEBUI_WIFI_SSID, pass[65] = CONFIG_WEBUI_WIFI_PASSWORD;
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof ssid; nvs_get_str(h, "wifi_ssid", ssid, &sz);
        sz = sizeof pass; nvs_get_str(h, "wifi_pass", pass, &sz);
        nvs_close(h);
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (ssid[0]) {
        wifi_sta_start(ssid, pass);
        ESP_LOGI(TAG, "STA connecting to '%s'", ssid);
    } else {
        wifi_ap_fallback();
    }
    ESP_RETURN_ON_ERROR(start_httpd(), TAG, "httpd");
    ESP_LOGI(TAG, "web ui ready");
    return ESP_OK;
}
