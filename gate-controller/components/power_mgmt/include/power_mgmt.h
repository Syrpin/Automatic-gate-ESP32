/*
 * power_mgmt.h — mains/battery supervision per plan p.9.1:
 *  step1 detect loss-of-mains (<10ms, GPIO ext wake),
 *  step2 save position to NVS (capacitor hold-up covers it),
 *  step3 flush event journal,
 *  step4 hand off to notify_tg (battery covers Wi-Fi transaction),
 *  step5 graceful degradation on low battery (camera/IR/WiFi-off policy),
 *  thermal interlock: charge-inhibit flag from NTC (hardware gate + soft flag).
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"

esp_err_t power_mgmt_init(void);
bool   power_mains_ok(void);
float  power_batt_volts(void);
bool   power_charge_inhibited(void);     /* T<batt min -> hardware+soft block */
uint8_t power_battery_pct(void);         /* crude OCV curve, wizard refines   */
void   power_urgent_save_and_notify(void);/* called from ZC-loss ISR context via queue */
void   power_mgmt_heartbeat(void);       /* WDT feed + periodic sampling      */
void   power_mains_watchdog(void);      /* ZC-gap check, call from ctrl tick  */
float  power_batt_temp_c(void);         /* NTC reading for telemetry          */
