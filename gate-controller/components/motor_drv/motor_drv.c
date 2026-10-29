/*
 * motor_drv.c — implementation notes:
 *  - Phase angle computed from throttle% -> delay after zero-cross using a
 *    one-shot esp_timer per half-cycle (50 Hz => 10 ms half; 60 Hz auto-detected
 *    by measuring ZC interval, replaces old AC_PERIOD magic constant).
 *  - Relay state machine enforces: open relays -> wait DEADTIME_MS -> fire triac
 *    path; reverse requires full stop + both relays off first.
 *  - This file compiles as a functional skeleton: the actual LEDC/MCPWM channel
 *    setup is stubbed behind CONFIG_GATE_SIM_MODE for bench testing w/o mains.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_check.h"
#include "hal_board.h"
#include "storage_nvs.h"
#include "motor_drv.h"

static const char *TAG = "mdrv";

#define DEADTIME_MS        60     /* relay make/break gap                  */
#define FAULT_NO_CURRENT_MS 900   /* triac on but CT silent => open circuit */

static struct {
    mdrv_state_t state;
    gate_dir_t   dir;
    uint8_t      throttle;       /* commanded %                            */
    uint8_t      applied;        /* post-ramp %                            */
    int64_t      energized_ms;   /* uptime since set() for watchdog        */
    bool         relay_seq_ok;
} s;

static void relays_apply(gate_dir_t d)
{
    gpio_set_level(PIN_RELAY_OPEN,  d == DIR_OPEN  ? 1 : 0);
    gpio_set_level(PIN_RELAY_CLOSE, d == DIR_CLOSE ? 1 : 0);
    /* K3 shunt must NEVER close while the triac path is active: it would
       bypass phase control (and short the triac branch). Drop it on every
       direction change; update() re-engages only at sustained full speed. */
    gpio_set_level(PIN_RELAY_SHUNT, 0);
}

static void safe_off(void)
{
    s.applied = 0;
    s.throttle = 0;
    gpio_set_level(PIN_TRIAC_PWM, 0);
    relays_apply(DIR_NONE);
    s.state = MDRV_IDLE;
}

esp_err_t motor_drv_init(void)
{
    memset(&s, 0, sizeof s);
    hal_board_safe_state();
    /* Zero-cross interrupt on PIN_MAINS_DETECT: */
    gpio_set_intr_type(PIN_MAINS_DETECT, GPIO_INTR_NEGEDGE);
    /* Phase B bring-up will do: gpio_install_isr_service(0); and route the
       PIN_MAINS_DETECT handler to motor_drv_zero_cross_us() below. Until then
       the hook is exercised from the ZC test pulse input (bench mode). */
    ESP_LOGI(TAG, "init ok (deadtime=%dms)", DEADTIME_MS);
    return ESP_OK;
}

/* ---------- zero-cross state: written ONLY by ISR, snapshotted for tasks ---- */
static volatile int64_t  s_zc_last_us;   /* last ZC timestamp (ISR-owned)      */
static volatile uint16_t s_half_ms = 10; /* measured mains half-period         */

/* ISR-visible hook (called from GPIO isr handler once wired): */
IRAM_ATTR void motor_drv_zero_cross_us(int64_t now_us)
{
    int64_t zl = s_zc_last_us;
    if (zl) {
        uint16_t hm = (uint16_t)((now_us - zl) / 1000);
        if (hm > 6 && hm < 12) s_half_ms = hm;   /* lock onto 42..83 Hz */
    }
    s_zc_last_us = now_us;
    /* Schedule firing for this half-cycle if running: */
    if (s.state != MDRV_IDLE && s.state != MDRV_FAULT && s.applied > 0) {
        uint32_t deg = (100 - s.applied) * 180u / 100u;      /* conduction % */
        int64_t delay_us = (int64_t)s_half_ms * 1000 * deg / 180;
        /* one-shot esp_timer arm omitted in skeleton (Phase B): fires TRIAC
           PIN_TRIAC_PWM high for ~200us at ZC+delay_us, low after. */
        (void)delay_us;
    }
}

/* Task-side accessor: single 16-bit read of a volatile updated only between
   valid ranges — atomic on Xtensa; also feeds the mains-loss watchdog. */
uint16_t motor_drv_mains_half_ms(void) { return s_half_ms; }
int64_t  motor_drv_last_zc_us(void)    { return s_zc_last_us; }

esp_err_t motor_drv_set(gate_dir_t dir, uint8_t throttle_pct)
{
    if (dir == DIR_NONE) { motor_drv_stop(false); return ESP_OK; }
    if (throttle_pct > storage_cfg()->duty_max_pct) throttle_pct = storage_cfg()->duty_max_pct;

    if (s.dir != dir && s.state != MDRV_IDLE) {
        /* reversal must go through stop+deadtime: reject, gate_logic retries */
        return ESP_ERR_INVALID_STATE;
    }
    if (s.state == MDRV_IDLE) {
        relays_apply(dir);                 /* close direction relay first...   */
        s.relay_seq_ok = false;            /* ...fire triac only after DEADTIME*/
        s.energized_ms = (int64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
        s.state = MDRV_RAMPING;
    } else if (dir != s.dir) {
        /* same-direction re-issue while relays are still settling must not
           restart the timer; opposite dir was rejected above */
        s.relay_seq_ok = false;
    }
    s.dir = dir;
    s.throttle = throttle_pct;
    return ESP_OK;
}

void motor_drv_stop(bool soft)
{
    if (!soft || s.state == MDRV_FAULT) { safe_off(); return; }
    s.state = MDRV_DECEL;   /* update() ramps applied->0 then safe_off */
}

void motor_drv_update(const TickPeriod_t *tp)
{
    switch (s.state) {
    case MDRV_IDLE: case MDRV_FAULT: break;
    case MDRV_RAMPING: {
        uint32_t up = storage_cfg()->ramp_up_ms;
        uint32_t t = (uint32_t)(tp->now_ms - s.energized_ms);
        /* relay settle gate: */
        if (t < DEADTIME_MS) break;
        s.relay_seq_ok = true;
        uint8_t target = s.throttle;
        uint16_t step = (up > 0) ? (uint16_t)(tp->period_ms * 100 / up) : 100;
        if (step == 0) step = 1;
        s.applied = (uint8_t)((s.applied + step >= target) ? target : s.applied + step);
        if (s.applied >= target) s.state = MDRV_RUNNING;
        break;
    }
    case MDRV_RUNNING:
        if (s.applied < s.throttle) s.applied = s.throttle;
        /* K3 shunt engagement is DEFERRED to Phase B hardware bring-up:
           switching under load welds contacts; needs zero-current window
           (triac current ~0 at ZC) + relay-cycle counter in NVS. Until then
           the gate runs phase-angle only — see README "Известные TODO". */
        break;
    case MDRV_DECEL: {
        uint16_t dn = storage_cfg()->ramp_down_ms;
        uint16_t step = (dn > 0) ? (uint16_t)(tp->period_ms * 100 / dn) : 100;
        if (step == 0) step = 1;
        s.applied = (s.applied <= step) ? 0 : s.applied - step;
        if (s.applied == 0) safe_off();
        break;
    }
    }
    /* Watchdog: energized but no current reported -> fault (stuck triac/wire).
       Cross-check value pushed by current_sense via gate_logic: */
    /* implemented in gate_logic_step (needs RMS sample) -> see there. */
}

mdrv_state_t motor_drv_state(void) { return s.state; }
gate_dir_t   motor_drv_direction(void) { return s.relay_seq_ok ? s.dir : DIR_NONE; }

bool motor_drv_relay_health(uint8_t *mask_out)
{
    uint8_t m = 0;
    m |= gpio_get_level(PIN_RELAY_OPEN)  ? 1 : 0;
    m |= gpio_get_level(PIN_RELAY_CLOSE) ? 2 : 0;
    m |= gpio_get_level(PIN_RELAY_SHUNT) ? 4 : 0;
    *mask_out = m;
    /* K1&K2 together is illegal (shoot-through of the direction H-branch);
       K3 asserted while idle means the shunt relay is welded/stuck. */
    if (m == 3) return false;
    if (s.state == MDRV_IDLE && (m & 4)) return false;
    return true;
}
