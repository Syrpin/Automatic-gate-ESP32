#include "esp_log.h"
#include "storage_nvs.h"
#include "notify_tg.h"
static const char*TAG="tg";
esp_err_t notify_tg_init(void){
    if(storage_get_pending_notify()){ ESP_LOGW(TAG,"pending outage notification -> resend"); }
    ESP_LOGI(TAG,"notify_tg stub; esp_http_client implementation in Phase D");
    return ESP_OK;
}
esp_err_t notify_tg_send(const char*t){(void)t;return ESP_OK;}
