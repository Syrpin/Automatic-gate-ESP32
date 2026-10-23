#include <string.h>
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
#define CT_LSB_TO_AMPS 0.0f /* placeholder until calibration fills NVS gain */
#define WIN_N 1024   /* window capacity; actual length = whole mains periods
                        (see win_len): 10 periods @50Hz, ~8 @60Hz at 10kSps;
                        real build swaps oneshot burst for ADC DMA */

static adc_oneshot_unit_handle_t uh;
static float rms_last;
static uint32_t over_since_ms; static bool over_latched; static bool inrush_hit;
static int64_t buf[WIN_N]; static size_t bi;

esp_err_t current_sense_init(void){
    adc_oneshot_unit_init_cfg_t uc={.unit_id=ADC_UNIT_1};
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&uc,&uh),TAG,"adc");
    adc_oneshot_chan_cfg_t cc={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_DEFAULT};
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(uh,CH_CURRENT_CT,cc),TAG,"chan");
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
        double acc=0; for(size_t i=0;i<wl;i++){double v=buf[i]-2048; acc+=v*v;}
        rms_last=(float)sqrt(acc/wl)*CT_LSB_TO_AMPS; /* calibrate in wizard */
        bi=0;
        const gate_config_t*c=storage_cfg();
        if(rms_last>c->i_rms_run_max){ if(!over_since_ms)over_since_ms=tp->now_ms;
            if(tp->now_ms-over_since_ms>=100) over_latched=true; }
        else { over_since_ms=0; over_latched=false; }
        if(rms_last>c->i_rms_peak_max) inrush_hit=true;
    }
}
float current_rms(void){return rms_last;}
bool current_over_run(uint32_t h){(void)h;return over_latched;}
bool current_over_inrush(void){return inrush_hit;}
