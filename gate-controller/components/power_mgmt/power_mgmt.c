#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"
#include "hal_board.h"
#include "storage_nvs.h"
#include "events_log.h"
#include "encoder.h"
#include "power_mgmt.h"
#ifdef CONFIG_GATE_SIM
#include "sim_hooks.h"
#endif

static const char *TAG="pwr";
#define MAINS_LOST_DEBOUNCE_MS 15

#define BATT_DIVIDER 2.0f
/* Hold-up health threshold: a 24V-class pack (or supercap stack) is considered
   serviceable above this voltage; below it the loss-of-mains budget is at risk. */
#define HOLDUP_MIN_V 24.0f
static adc_oneshot_unit_handle_t uh;
static volatile bool mains_ok=true, mains_pending_down;
static uint32_t mains_change_ms;
static float batt_v=0; static uint32_t last_sample_ms;

/* ZC-loss detection reuses PIN_MAINS_DETECT line presence: while mains present
   comparator emits pulses at 100Hz; silence >MAIN debounce => lost. Sampled in
   heartbeat (1s) AND pushed fast-path by a dedicated esp_timer in Phase B. */
static void mark_mains(bool up){
    if(up==mains_ok) return;
    mains_ok=up; mains_change_ms=(uint32_t)(esp_timer_get_time()/1000);
    events_log_write(EV_POWER,"mains %s",up?"restored":"LOST");
    if(!up) power_urgent_save_and_notify();
}

esp_err_t power_mgmt_init(void){
    adc_oneshot_unit_init_cfg_t uc={.unit_id=ADC_UNIT_1};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&uc,&uh),TAG,"adc");
    adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_DEFAULT};
    /* Battery telemetry was descoped (decision 2026): the hold-up source is a
       health-check only, so we configure just the pack/divider channel and do
       NOT sample an NTC battery-temperature channel. The thermal interlock
       remains a hardware charge-gate; power_charge_inhibited() stays a stub. */
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(uh,CH_BATTERY,cc),TAG,"batt");
    return ESP_OK;
}

static void sample(void){
    int raw; if(adc_oneshot_read(uh,CH_BATTERY,&raw)==ESP_OK)
        batt_v=raw*3.3f/4095.0f*BATT_DIVIDER; /* set divider in sdkconfig later */
}

bool power_mains_ok(void){ return mains_ok; }
float power_batt_volts(void){ return batt_v; }
bool power_charge_inhibited(void){ /* hardware charge-gate is authoritative; soft flag stub */ return false; }

/* Battery state-of-charge telemetry was descoped (decision 2026): the hold-up
   source only needs to be healthy enough to cover loss-of-mains save + one
   Telegram transaction. We expose a voltage sanity check instead of SoC%. */
bool power_holdup_ok(void){ return batt_v >= HOLDUP_MIN_V; }

void power_urgent_save_and_notify(void){
    /* steps 2-3 of the UPS budget (p.9.1): position first, then journal */
    storage_save_position(encoder_position(),0,true);
    events_log_flush();
    /* step 4 handled by notify_tg task which observes EV_POWER LOST event;
       if network also dark, pending flag ensures late delivery after reboot */
    storage_set_pending_notify(!mains_ok);
}

void power_mgmt_heartbeat(void){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
#ifdef CONFIG_GATE_SIM
    { bool ok; if(sim_mains_state(&ok)) mark_mains(ok); }
#endif
    if(now-last_sample_ms>1000){ last_sample_ms=now; sample(); }
    /* mains pulse watchdog wired in Phase B (esp_timer measuring ZC gaps);
       skeleton keeps mains_ok from boot state */
    (void)mains_pending_down;(void)mains_change_ms;(void)mark_mains;
}
