#include <string.h>
#include <stdlib.h>   /* abs */
#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_check.h"
#include "hal_board.h"
#include "storage_nvs.h"
#include "motor_drv.h"
#include "current_sense.h"
#ifdef CONFIG_GATE_SIM
#include "sim_hooks.h"
#endif

static const char *TAG="ct";

/* ---- sensor front-end constants (see SCHEMATIC.md §2) --------------------
 * ACS712-xxA: ratiometric, Vout = VCC/2 ± K·I, K = 185/100/66 mV/A for the
 * 05B/20A/30A variants.  Gain stage: non-inverting x G about VCC/2 so full
 * swing fits the ESP32-S3 ADC1 input range at ATTEN_DB_12 (~0..3.1 V):
 *   usable half-rail span U = 3.1 - VCC/2;  I_full = sensor max current
 *   G = U / (K·I_full)      e.g. ACS712-30A @ VCC=3.3:  1.45/(0.066·30)=0.73
 * Midrail in raw LSB is MEASURED at init (motor off) — tracks VCC exactly,
 * no resistor-divider error. */
#define ADC_VREF_MV      3100   /* ATTEN_DB_12 usable top, mV               */
#define ADC_LSB_MV       (ADC_VREF_MV / 4095.0f)
#define WIN_N 1024   /* window capacity; actual length = whole mains periods
                        (see win_len): 10 periods @50Hz, ~8 @60Hz at 10kSps;
                        real build swaps oneshot burst for ADC DMA */

static adc_oneshot_unit_handle_t uh;
static float rms_last;
static int32_t s_mid_raw = 2048;        /* measured midrail, LSB            */
static float   s_amps_per_lsb = 0.0f;   /* calibration constant, A/LSB      */
static uint16_t s_gain_x1000 = 1000;    /* front-end gain G, 0.001 units    */
static uint16_t s_k_mv_per_a = 100;     /* sensor K, mV/A                   */
static uint16_t s_vcc_mv = 3300;        /* analog supply of the sensor      */
static uint32_t over_since_ms; static bool over_latched; static bool inrush_hit;
static int64_t buf[WIN_N]; static size_t bi;

esp_err_t current_sense_init(void){
    adc_oneshot_unit_init_cfg_t uc={.unit_id=ADC_UNIT_1};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&uc,&uh),TAG,"adc");
    adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_DEFAULT};
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(uh,CH_CURRENT_CT,cc),TAG,"chan");
    return ESP_OK;
}

/* Configure the sensor front end once at boot (values from NVS config):
 *   k_mv_per_a : sensor transfer function, mV/A (ACS712-05B=185, 20A=100,
 *                30A=66; hall CT modules per datasheet)
 *   vcc_mv     : sensor analog supply (ratiometric -> midrail tracks it)
 *   i_full_a   : current that must map to full ADC scale (e.g. 30 for a
 *                30 A part) — used ONLY to derive G when gain_x1000==0:
 *                   G = (ADC_VREF_MV - vcc_mv/2) / (k_mv_per_a * i_full_a)
 *   gain_x1000 : explicit front-end gain in 0.001 units (0 -> compute as above)
 * Resulting constant:  A/LSB = 1 / (K * G * LSB_mV)  [all in mV units] */
void current_sense_set_front_end(uint16_t k_mv_per_a, uint16_t vcc_mv,
                                 uint16_t i_full_a, uint16_t gain_x1000)
{
    if (k_mv_per_a) s_k_mv_per_a = k_mv_per_a;
    if (vcc_mv)     s_vcc_mv     = vcc_mv;
    if (gain_x1000) {
        s_gain_x1000 = gain_x1000;
    } else if (i_full_a && s_k_mv_per_a) {
        double g = (double)(ADC_VREF_MV - s_vcc_mv/2) /
                   ((double)s_k_mv_per_a * i_full_a);
        if (g < 0.05) g = 0.05;          /* absurd budget -> clamp + warn   */
        if (g > 20.0) g = 20.0;
        s_gain_x1000 = (uint16_t)(g * 1000.0 + 0.5);
    }
    /* ADC deviation d[LSB] about midrail -> current:
     *   I = d * LSB_mV / (K[mV/A] * G)      =>  A/LSB = LSB_mV/(K*G)      */
    s_amps_per_lsb = (float)(((double)ADC_LSB_MV) /
                        ((double)s_k_mv_per_a * (s_gain_x1000/1000.0)));
    ESP_LOGI(TAG,"CT frontend: K=%u mV/A VCC=%u mV G=%.3f -> %.4f A/LSB",
             s_k_mv_per_a, s_vcc_mv, s_gain_x1000/1000.0, (double)s_amps_per_lsb);
}

/* Measure midrail (motor OFF): average of 64 samples right after init. */
esp_err_t current_sense_calibrate_midrail(void){
    if(!uh) return ESP_ERR_INVALID_STATE;
    int64_t acc=0; int ok=0;
    for(int i=0;i<64;i++){ int raw;
        if(adc_oneshot_read(uh,CH_CURRENT_CT,&raw)==ESP_OK){acc+=raw;ok++;} }
    if(ok<32) return ESP_FAIL;                    /* ADC not settling       */
    s_mid_raw=(int32_t)(acc/ok);
    /* sanity: ratiometric midrail must sit near VCC/2 within ~12 % */
    int32_t expect=(int32_t)(4095.0*s_vcc_mv/2.0/ADC_VREF_MV);
    if(abs((int)s_mid_raw-expect)>expect/8){
        ESP_LOGW(TAG,"midrail %ld far from expected %ld (sensor/VCC?)",(long)s_mid_raw,(long)expect);
    }
    ESP_LOGI(TAG,"midrail=%ld LSB (%.0f mV)",(long)s_mid_raw,(float)s_mid_raw*ADC_LSB_MV);
    return ESP_OK;
}

/* RMS window = integer number of mains half-periods so the average closes
 * exactly on zero-crossings (no partial-cycle bias). At 10 kSps:
 *   50 Hz -> 200 samples/half, WIN_N/200 = 5 halves  = 2.5 periods (100 ms)
 *   60 Hz -> ~166 samples/half, WIN_N/166 = 6 halves = 3 periods   (100 ms)
 * Both land on whole half-cycles; Phase B (DMA ring) may widen to 10 periods. */
static size_t win_len(void)
{
    uint32_t hu = motor_drv_half_period_us();      /* us, e.g. 10000/8333 */
    if (hu == 0) hu = 10000;                       /* safe default 50 Hz  */
    size_t per_half = (size_t)(hu / 100);          /* samples @10kSps     */
    if (per_half < 2) per_half = 2;
    size_t n = (WIN_N / per_half) * per_half;      /* whole halves only   */
    return n >= per_half ? n : per_half;
}

void current_sense_sample(const TickPeriod_t *tp){
#ifdef CONFIG_GATE_SIM
    /* bare-board/HIL: injected or plant-model amps replace the floating ADC */
    float amps;
    if(sim_current_value(&amps)){ rms_last=amps; const gate_config_t*c=storage_cfg();
        if(amps>c->i_rms_run_max){ if(!over_since_ms)over_since_ms=tp->now_ms;
            if(tp->now_ms-over_since_ms>=100) over_latched=true; }
        else { over_since_ms=0; over_latched=false; }
        if(amps>c->i_rms_peak_max) inrush_hit=true;
        return; }
#endif
    const size_t wl = win_len();
    /* skeleton: oneshot burst per tick (Phase B swaps to continuous DMA ring) */
    int raw; if(adc_oneshot_read(uh,CH_CURRENT_CT,&raw)!=ESP_OK) return;
    if(bi<wl) buf[bi++]=raw;
    else { /* close window -> RMS about mid-rail */
        double acc=0; for(size_t i=0;i<wl;i++){double v=buf[i]-(double)s_mid_raw; acc+=v*v;}
        rms_last=(float)sqrt(acc/wl)*s_amps_per_lsb; /* exact-cycle window  */
        bi=0;
        const gate_config_t*c=storage_cfg();
        if(rms_last>c->i_rms_run_max){ if(!over_since_ms)over_since_ms=tp->now_ms;
            if(tp->now_ms-over_since_ms>=100) over_latched=true; }
        else { over_since_ms=0; over_latched=false; }
        if(rms_last>c->i_rms_peak_max) inrush_hit=true;
    }
}
float current_rms(void){return rms_last;}
float current_sense_fullscale_a(void){
    /* one-sided span: counts from measured midrail to either rail end,
     * converted through the calibrated K*G chain (~I_full by design)      */
    int32_t up   = 4095 - s_mid_raw;
    int32_t down = s_mid_raw;
    int32_t span = (up < down ? up : down);
    return (float)span * s_amps_per_lsb;
}
bool current_over_run(uint32_t h){(void)h;return over_latched;}
bool current_over_inrush(void){return inrush_hit;}
