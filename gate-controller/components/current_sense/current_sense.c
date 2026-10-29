#include <string.h>
#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_check.h"
#include "hal_board.h"
#include "storage_nvs.h"
#include "current_sense.h"

static const char *TAG="ct";
/* CT gain: NVS-calibrated amps per LSB of the centered ADC window. 0 means
   "not calibrated yet" -> protection thresholds are meaningless, so we fail
   safe by NOT tripping stall/pinch on current and logging a one-time warning
   (README TODO #2: filled by the Phase E calibration wizard). */
/* initial gain = "uncalibrated". Overwritten from NVS cfg at init and by the
   wizard via current_sense_set_gain(). Never hard-code a guessed value here:
   a wrong gain silently mis-trips pinch protection (worse than none). */
#define CT_LSB_TO_AMPS_UNCAL 0.0f
#define WIN_N 1024   /* ~10 periods at 50Hz/10kSps; real build uses ADC DMA */

static adc_oneshot_unit_handle_t uh;
static float rms_last;
static uint32_t over_since_ms; static bool over_latched; static bool inrush_hit;
static int32_t buf[WIN_N]; static size_t bi;
static float s_gain = CT_LSB_TO_AMPS_UNCAL;

esp_err_t current_sense_init(void){
    adc_oneshot_unit_init_cfg_t uc={.unit_id=ADC_UNIT_1};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&uc,&uh),TAG,"adc");
    adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_WIDTH_BIT_DEFAULT};
    s_gain = storage_cfg()->ct_gain_a_per_lsb;   /* restored from NVS */
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(uh,CH_CURRENT_CT,&cc),TAG,"chan");
    if(s_gain<=0.0f)
        ESP_LOGW(TAG,"CT gain uncalibrated: overcurrent protection INACTIVE");
    return ESP_OK;
}

void current_sense_sample(const TickPeriod_t *tp){
    /* skeleton: oneshot burst per tick (Phase B swaps to continuous DMA ring) */
    int raw; if(adc_oneshot_read(uh,CH_CURRENT_CT,&raw)!=ESP_OK) return;
    if(bi<WIN_N) buf[bi++]=raw;
    else { /* close window -> RMS about mid-rail */
        double acc=0; for(size_t i=0;i<WIN_N;i++){double v=buf[i]-2048; acc+=v*v;}
        rms_last=(float)sqrt(acc/WIN_N)*s_gain; /* calibrate in wizard */
        bi=0;
        const gate_config_t*c=storage_cfg();
        if(s_gain>0.0f && rms_last>c->i_rms_run_max){
            if(!over_since_ms)over_since_ms=tp->now_ms;
            if(tp->now_ms-over_since_ms>=100) over_latched=true; }
        else { over_since_ms=0; over_latched=false; }
        if(s_gain>0.0f && rms_last>c->i_rms_peak_max) inrush_hit=true;
    }
}
float current_rms(void){return rms_last;}
bool current_over_run(uint32_t h){(void)h;return over_latched;}
bool current_over_inrush(void){return inrush_hit;}
void current_sense_set_gain(float amps_per_lsb){
    if(amps_per_lsb>0.0f){ s_gain=amps_per_lsb; ESP_LOGI(TAG,"CT gain=%.6g A/LSB",
        (double)amps_per_lsb); } }
