/*
 * camera_pipeline.h — event-driven capture control (plan p.10): idle OFF or
 * 5fps QVGA motion-watch; on trigger raise to 15-25fps VGA and run vision_core.
 * Skeleton API only; esp_cam init lands in Phase C.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum { CAM_OFF, CAM_IDLE_LOWFPS, CAM_ACTIVE } cam_mode_t;
esp_err_t camera_pipeline_init(void);
esp_err_t camera_set_mode(cam_mode_t m);   /* respects battery policy via power_mgmt */
cam_mode_t camera_get_mode(void);
bool camera_frame_ready(void);             /* frame handle passed to vision_core */
