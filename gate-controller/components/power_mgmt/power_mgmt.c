#include "freertos/FreeRTOS.h"
#include <math.h>
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
#include "motor_drv.h"
#include "power_mgmt.h"

static const char *TAG="pwr";
#define MAINS_LOST_DEBOUNCE_MS 15
/* Loss-of-mains watchdog: ZC pulses arrive every ~10 ms (50 Hz half-period).
   If the last-edge age exceeds 3.5 half-periods with no pulse, mains is gone.
   Checked from the fast ctrl loop path via power_mains_watchdog() called in
   heartbeat AND from gate_logic step; worst-case detection < 35 ms. */
#define MAINS_ZC_GAP_MS 35
#define BATT_LOW_PCT    10   /* graceful degradation threshold (step 5) */

#define BATT_DIVIDER 2.0f
static adc_oneshot_unit_handle_t uh;
static volatile bool mains_ok=true, mains_pending_down;
static uint32_t mains_change_ms;
static float batt_v=0; static uint32_t last_sample_ms;
static float ntc_c=-999;           /* last NTC temperature, C; -999 = unknown */

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
    adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_WIDTH_BIT_DEFAULT};
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(uh,CH_BATTERY,&cc),TAG,"batt");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(uh,CH_TEMP_BATT,&cc),TAG,"ntc");
    /* seed mains state from the ZC watchdog itself: if no zero-cross has ever
       been seen yet, assume present (boot default) and let the watchdog decide */
    return ESP_OK;
}

static void sample(void){
    int raw; if(adc_oneshot_read(uh,CH_BATTERY,&raw)==ESP_OK)
        batt_v=raw*3.3f/4095.0f*BATT_DIVIDER; /* set divider in sdkconfig later */
    /* NTC 10k B3950 with 10k series pull-up to 3V3: midpoint -> R_ntc = R*... */
    if(adc_oneshot_read(uh,CH_TEMP_BATT,&raw)==ESP_OK){
        float v = raw*3.3f/4095.0f;
        if(v>0.001f && v<3.299f){
            float r = 10000.0f*v/(3.3f-v);           /* ohms */
            ntc_c = 1.0f/(1.0f/298.15f+logf(r/10000.0f)/3950.0f)-273.15f;
        } else ntc_c = -999;
    }
}

bool power_mains_ok(void){ return mains_ok; }
float power_batt_volts(void){ return batt_v; }
bool power_charge_inhibited(void){
    /* LiFePO4 charge window 0..45 C; unknown sensor (-999) => do NOT inhibit
       (hardware thermal fuse is the backstop when ADC reading is absent). */
    if(ntc_c<-900) return false;
    return ntc_c < 0.0f || ntc_c > 45.0f;
}
uint8_t power_battery_pct(void){
    /* LiFePO4-ish OCV; replace with fuel gauge when chosen (p.11 question closed as non-critical) */
    if(batt_v<=0) return 0;
    int pct=(int)((batt_v-26.4f)/(29.2f-26.4f)*100);
    return pct<0?0:pct>100?100:(uint8_t)pct;
}

void power_urgent_save_and_notify(void){
    /* steps 2-3 of the UPS budget (p.9.1): position first, then journal */
    storage_save_position(encoder_position(),0,true);
    events_log_flush();
    /* step 4 handled by notify_tg task which observes EV_POWER LOST event;
       if network also dark, pending flag ensures late delivery after reboot */
    storage_set_pending_notify(!mains_ok);
}

/* Loss-of-mains watchdog, evaluated against the last ZC edge recorded by
   motor_drv's ISR. Called from heartbeat (1 s) — but ALSO must be called at
   least every MAINS_ZC_GAP_MS ms from a fast path for the <10 ms budget
   (plan p.9.1 step 1); gate_logic_step() invokes it each ctrl tick. */
void power_mains_watchdog(void){
    int64_t zc_us = motor_drv_last_zc_us();
    if(zc_us==0) return;              /* no ZC ever seen: keep boot assumption */
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    uint32_t age=(uint32_t)((esp_timer_get_time()-zc_us)/1000);
    if(mains_ok && age>MAINS_ZC_GAP_MS){
        if(!mains_pending_down){ mains_pending_down=true; mains_change_ms=now; }
        else if(now-mains_change_ms>=MAINS_LOST_DEBOUNCE_MS){
            mains_pending_down=false; mark_mains(false);
        }
    } else {
        mains_pending_down=false;
        if(!mains_ok && age<=MAINS_ZC_GAP_MS) mark_mains(true);
    }
}

void power_mgmt_heartbeat(void){
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(now-last_sample_ms>1000){ last_sample_ms=now; sample(); }
    power_mains_watchdog();
    /* step 5 graceful degradation: on low battery ask higher layers to shed
       loads (camera/IR/Wi-Fi). Policy lives in camera_pipeline/notify_tg via
       this query; here we only log the transition once. */
    static bool low_logged;
    bool low = power_battery_pct()<BATT_LOW_PCT;
    if(low && !low_logged){ low_logged=true;
        events_log_write(EV_POWER,"battery LOW %d%%: shed camera/IR",(int)power_battery_pct()); }
    if(!low) low_logged=false;
}

float power_batt_temp_c(void){ return ntc_c; }
