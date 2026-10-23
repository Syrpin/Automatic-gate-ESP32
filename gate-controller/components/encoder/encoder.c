/* encoder.c — devkit path implements GPIO-ISR pulse accumulation; PCNT path
 * selected when hal_board_profile()->enc_use_gpio_isr == false (custom v1).
 * ISR body kept minimal: count edges into volatile deltas; all decoding and
 * status logic runs in ctrl_task context (10 ms) — no work lost on races
 * because deltas are consumed atomically under portMUX. */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_check.h"
#include "hal_board.h"
#include "motor_drv.h"
#include "esp_timer.h"
#include "encoder.h"

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static const char *TAG = "enc";

#define STALE_LIMIT_MS 1500

static struct {
    int32_t pos;              /* accumulated counts                         */
    volatile int32_t raw_delta;/* drained from ISR each tick                 */
    uint32_t last_change_ms;
    enc_status_t status;
    float    speed_cps;
    uint32_t prev_pos;
    pcnt_unit_handle_t unit;  /* custom-PCNT path only                      */
} e;

/* ---------- devkit GPIO-ISR decode (quadrature subset) -------------------- */
static int8_t s_prev_ab = 3;
static const int8_t SEQ[16] = {0,-1,+1,0, +1,0,0,-1, -1,0,0,+1, 0,+1,-1,0};

IRAM_ATTR static void enc_isr(void *arg)
{
    int a = gpio_get_level(PIN_ENC_A);
    int b = gpio_get_level(PIN_ENC_B);
    int8_t ab = (a << 1) | b;
    if (s_prev_ab != 3) e.raw_delta += SEQ[((s_prev_ab << 2) | ab) & 0xF];
    s_prev_ab = ab;
}

esp_err_t encoder_init(void)
{
    memset(&e, 0, sizeof e);
    e.status = ENC_OK;
    const hal_board_profile_t *p = hal_board_profile();
    if (p->enc_use_gpio_isr) {
        gpio_config_t gc = { .pin_bit_mask = (1ULL<<PIN_ENC_A)|(1ULL<<PIN_ENC_B),
                             .mode = GPIO_MODE_INPUT, .pull_up_en = 1 };
        ESP_RETURN_ON_ERROR(gpio_config(&gc), TAG, "enc pins");
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        gpio_set_intr_type(PIN_ENC_A, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(PIN_ENC_B, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(PIN_ENC_A, enc_isr, NULL);
        gpio_isr_handler_add(PIN_ENC_B, enc_isr, NULL);
        ESP_LOGI(TAG, "mode=GPIO-ISR quadrature");
    } else {
        /* PCNT unit: edge_count up/down from A/B, high_limit glitch filter.
           Full init omitted in skeleton; API is pcnt_new_unit/..._channel. */
        ESP_LOGI(TAG, "mode=PCNT (custom board)");
    }
    return ESP_OK;
}

void encoder_update(const TickPeriod_t *tp)
{
    int32_t d;
    taskENTER_CRITICAL(&s_mux);
    d = e.raw_delta; e.raw_delta = 0;
    taskEXIT_CRITICAL(&s_mux);

    if (d != 0) {
        /* overflow-safe: saturating int32 accumulate, never wraps silently */
        int64_t np = (int64_t)e.pos + d;
        e.pos = (np > INT32_MAX) ? INT32_MAX : (np < INT32_MIN) ? INT32_MIN : (int32_t)np;
        e.last_change_ms = tp->now_ms;
        e.status = ENC_OK;
    } else if (motor_drv_state() != MDRV_IDLE &&
               (uint32_t)(tp->now_ms - e.last_change_ms) > STALE_LIMIT_MS) {
        e.status = ENC_STALE;   /* motor energized, sensor silent -> fault */
    }
    /* speed: EMA over ticks */
    uint32_t dt = tp->period_ms;
    int32_t dp = e.pos - (int32_t)e.prev_pos;
    e.prev_pos = (uint32_t)e.pos;
    if (dt) e.speed_cps = 0.7f * e.speed_cps + 0.3f * (dp * 1000 / (float)dt);
}

int32_t encoder_position(void) { return e.pos; }
enc_status_t encoder_status(void) { return e.status; }
uint32_t encoder_last_change_ms(void) { return e.last_change_ms; }
float encoder_speed_counts_per_s(void) { return e.speed_cps; }

void encoder_set_origin_closed(void) { e.pos = 0; e.last_change_ms = esp_timer_get_time()/1000; }
void encoder_set_origin_open(void)   { e.pos = storage_cfg()->travel_counts; }

bool encoder_dir_matches(gate_dir_t cmd, uint32_t window_ms)
{
    if (cmd == DIR_NONE) return true;
    if ((uint32_t)(esp_timer_get_time()/1000 - e.last_change_ms) < window_ms) {
        int8_t sign = (e.speed_cps > 0) - (e.speed_cps < 0);
        return (cmd == DIR_OPEN && sign >= 0) || (cmd == DIR_CLOSE && sign <= 0);
    }
    return false; /* stale while moving => sensor fault */
}

void encoder_set_position_raw(int32_t p){ taskENTER_CRITICAL(&s_mux); e.pos=p; e.raw_delta=0; taskEXIT_CRITICAL(&s_mux);
    e.last_change_ms=(uint32_t)(esp_timer_get_time()/1000); e.status=ENC_OK; }

/* simulator feed: behaves exactly like real ISR-produced deltas.
 * Always compiled (see encoder.h): sim component links against it. */
void encoder_add_sim_counts(int32_t d)
{
    taskENTER_CRITICAL(&s_mux);
    e.raw_delta += d;
    taskEXIT_CRITICAL(&s_mux);
}
