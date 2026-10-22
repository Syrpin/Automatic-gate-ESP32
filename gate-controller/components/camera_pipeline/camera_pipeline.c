#include "esp_log.h"
#include "esp_check.h"
#include "camera_pipeline.h"
static const char*TAG="cam";
static cam_mode_t mode=CAM_OFF;
esp_err_t camera_pipeline_init(void){ ESP_LOGI(TAG,"OV5640 DVP init deferred to Phase C"); return ESP_OK; }
esp_err_t camera_set_mode(cam_mode_t m){ mode=m; ESP_LOGI(TAG,"mode=%d",(int)m); return ESP_OK; }
cam_mode_t camera_get_mode(void){return mode;}
bool camera_frame_ready(void){return false;}
