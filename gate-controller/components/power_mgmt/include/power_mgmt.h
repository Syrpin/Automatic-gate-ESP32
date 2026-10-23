/*
 * power_mgmt.h — mains supervision + hold-up health per plan p.9.1:
 *  step1 detect loss-of-mains (<10ms, GPIO ext wake),
 *  step2 save position to NVS (capacitor/battery hold-up covers it),
 *  step3 flush event journal,
 *  step4 hand off to notify_tg (hold-up source covers Wi-Fi transaction).
 * Battery state-of-charge telemetry was descoped (decision 2026): the hold-up
 * source is a health-check only (voltage sanity), not a monitored pack. The
 * thermal charge-interlock is enforced in hardware; power_charge_inhibited()
 * is a soft-flag stub.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "common_types.h"

esp_err_t power_mgmt_init(void);
bool   power_mains_ok(void);
float  power_batt_volts(void);
bool   power_charge_inhibited(void);     /* hardware charge-gate + soft flag  */
bool   power_holdup_ok(void);            /* hold-up source health (voltage) —
                                            battery SoC telemetry descoped    */
void   power_urgent_save_and_notify(void);/* called from ZC-loss ISR context via queue */
void   power_mgmt_heartbeat(void);       /* WDT feed + periodic sampling      */
