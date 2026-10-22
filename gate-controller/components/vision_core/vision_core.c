#include "esp_log.h"
#include "gate_logic.h"
#include "vision_core.h"
static const char*TAG="vision";
static veh_class_t last=VEH_UNKNOWN;
void vision_process_frame(const uint8_t*f,uint16_t w,uint16_t h,uint32_t t){
    (void)f;(void)w;(void)h;(void)t;
    /* Phase C/D: detector->tracker->evidence->score. Emits:
       gate_vision_event(1|2|3, vehicle_id) */
}
veh_class_t vision_last_vehicle(void){return last;}
