/* beep.c — table-driven, non-blocking patterns driven from the 50 ms beep
 * task (gate_logic_beep_tick -> beep_update). Tone via LEDC on PIN_BEEP;
 * duty=0 == silent, so "tone" vs "silence" is one register write per slot. */
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "hal_board.h"
#include "beep.h"

static const char *TAG = "beep";

#define BEEP_FREQ_HZ   2500
#define LEDC_RES_BITS  8
#define SLOT_MS        80

/* Pattern encoding: each byte = one step; N>0 -> tone for N slots then one
   gap slot; 0 -> silence for one slot; 0xFF = end of pattern sentinel. */
static const uint8_t PAT_NONE[]    = {0xFF};
static const uint8_t PAT_SHORT[]   = {1,0xFF};                 /* key press    */
static const uint8_t PAT_CONFIRM[] = {1,0,1,0,1,0xFF};         /* cmd accepted */
static const uint8_t PAT_WARN[]    = {3,0,3,0xFF};             /* fault warn   */
static const uint8_t PAT_ERR[]     = {1,0,1,0,1,0,1,0,1,0,1,0xFF}; /* latched FAULT */
static const uint8_t PAT_MOVE[]    = {1,0,0,1,0,0,0xFF};       /* while moving */

static struct {
    const uint8_t *pat;
    size_t i;
    int8_t slots_left;   /* tone slots remaining in current symbol */
    bool   gap_pending;  /* play one gap slot after a tone run     */
    bool   on;
} B;

void beep_start(const uint8_t *pattern){ B.pat=pattern?pattern:PAT_NONE; B.i=0;
                                         B.slots_left=0; B.gap_pending=false; B.on=false; }

esp_err_t beep_init(void)
{
    ledc_timer_config_t tc = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_1,          /* TIMER_0 reserved for RMT/WS2812 */
        .freq_hz = BEEP_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&tc), TAG, "timer");
    ledc_channel_config_t cc = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel    = LEDC_CHANNEL_6,        /* ch0..4 left free for future PWM */
        .timer_sel  = LEDC_TIMER_1,
        .gpio_num   = PIN_BEEP,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&cc), TAG, "chan");
    beep_start(PAT_NONE);
    ESP_LOGI(TAG,"init %dHz",BEEP_FREQ_HZ);
    return ESP_OK;
}

static void set_tone(bool on){
    if(on==B.on) return;
    B.on=on;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_6, on?(1<<(LEDC_RES_BITS-1)):0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_6);
}

void beep_short(void)   { beep_start(PAT_SHORT); }
void beep_confirm(void) { beep_start(PAT_CONFIRM); }
void beep_warn(void)    { beep_start(PAT_WARN); }
void beep_error(void)   { beep_start(PAT_ERR); }
/* looping patterns: only (re)arm when not already running that pattern */
void beep_move(void)    { if(B.pat!=PAT_MOVE)  beep_start(PAT_MOVE); }
void beep_fault_loop(void){ if(B.pat!=PAT_ERR) beep_start(PAT_ERR); }
void beep_stop_pattern(void){
    if(B.pat==PAT_MOVE||B.pat==PAT_ERR) beep_start(PAT_NONE);
}

void beep_update(uint32_t dt_ms)
{
    static uint32_t acc;
    acc += dt_ms;
    while (acc >= SLOT_MS) {
        acc -= SLOT_MS;
        if (B.pat==PAT_NONE || !B.pat) { set_tone(false); continue; }

        if (B.slots_left > 0) {            /* inside a tone run */
            set_tone(true);
            if (--B.slots_left == 0) B.gap_pending = true;
        } else if (B.gap_pending) {        /* mandatory gap after tone run */
            set_tone(false); B.gap_pending = false;
        } else {                           /* fetch next symbol */
            uint8_t sym = B.pat[B.i];
            if (sym == 0xFF) {             /* end: loop move/fault, else stop */
                if (B.pat==PAT_MOVE || B.pat==PAT_ERR) { B.i=0; }
                else { beep_start(PAT_NONE); }
                continue;
            }
            B.i++;
            if (sym == 0) set_tone(false);
            else B.slots_left = (int8_t)sym;
        }
    }
}
