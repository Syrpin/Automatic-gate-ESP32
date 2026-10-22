/*
 * vision_core.h — scenarios A/B as explicit FSM over scored evidence:
 *  A: car-detected -> track(Kalman)+polygon zone -> [plate, face, fingerprint,
 *     lights] evidence -> weighted score >= thresh within 2s -> APPROACH_OK.
 *  B: tracked ID crosses stop-line -> rear-lights blob pair + identity confirm
 *     + bbox left aperture (hysteresis) -> LEFT_CLEAR.
 * Model loading from model_a/model_b partitions with hot-swap is Phase F.
 */
#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum { VEH_UNKNOWN=0, VEH_WHITELISTED } veh_class_t;
void vision_process_frame(const uint8_t *rgb565, uint16_t w, uint16_t h, uint32_t now_ms);
veh_class_t vision_last_vehicle(void);
