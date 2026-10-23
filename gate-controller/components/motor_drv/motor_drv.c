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
    int64_t      zc_last_us;     /* last zero-cross timestamp              */
    uint32_t     half_us;        /* effective mains half-period, microsec. */
    mains_freq_sel_t freq_sel;   /* regional: AUTO / 50 / 60               */
    uint16_t     meas_hz;        /* ZC-measured line frequency (AUTO latch)*/
    uint8_t      meas_ok_cnt;    /* consecutive in-band ZC measurements    */
    int64_t      fired_us;       /* when triac pulse was scheduled         */
    int64_t      energized_ms;   /* uptime since set() for watchdog        */
    bool         relay_seq_ok;
} s;

/* Exact half-period in us for an explicit selection (no ms truncation:
 * 60 Hz => 8333 us, not 8000 — firing-angle error would be ~4%). */
static uint32_t half_us_for(mains_freq_sel_t sel)
{
    switch (sel) {
    case MAINS_FREQ_50HZ: return 10000;
    case MAINS_FREQ_60HZ: return 8333;
    default:              return 0;   /* AUTO: caller decides              */
    }
}

uint32_t motor_drv_half_period_us(void) { return s.half_us; }

void motor_drv_set_mains_freq(mains_freq_sel_t sel)
{
    s.freq_sel = sel;
    s.meas_ok_cnt = 0;                /* re-detect on any selection change */
    if (sel == MAINS_FREQ_AUTO) {
        s.meas_hz = 0;
        /* provisional safe fallback = nominal 50 Hz until ZC locks (ISR) */
        s.half_us = half_us_for(MAINS_FREQ_50HZ);
    } else {
        s.half_us = half_us_for(sel);
        s.meas_hz = (uint16_t)sel;    /* explicit pick is authoritative    */
    }
    ESP_LOGI(TAG, "mains freq := %s (half=%uus)",
             sel == MAINS_FREQ_50HZ ? "50Hz" :
             sel == MAINS_FREQ_60HZ ? "60Hz" : "AUTO",
             (unsigned)s.half_us);
}
mains_freq_sel_t motor_drv_mains_freq(void) { return s.freq_sel; }
uint16_t motor_drv_measured_hz(void) { return s.meas_hz; }

static void relays_apply(gate_dir_t d)
{
    gpio_set_level(PIN_RELAY_OPEN,  d == DIR_OPEN  ? 1 : 0);
    gpio_set_level(PIN_RELAY_CLOSE, d == DIR_CLOSE ? 1 : 0);
    /* K3 shunt only engaged at sustained full speed by update() */
    if (d == DIR_NONE) gpio_set_level(PIN_RELAY_SHUNT, 0);
}

static void safe_off(void)
{
    s.applied = 0;
    gpio_set_level(PIN_TRIAC_PWM, 0);
    relays_apply(DIR_NONE);
    s.state = MDRV_IDLE;
}

esp_err_t motor_drv_init(void)
{
    memset(&s, 0, sizeof s);
    /* Regional selection from NVS config (Kconfig default on first boot). */
    motor_drv_set_mains_freq(storage_cfg()->mains_freq);
    hal_board_safe_state();
    /* Zero-cross interrupt on PIN_MAINS_DETECT: */
    gpio_set_intr_type(PIN_MAINS_DETECT, GPIO_INTR_NEGEDGE);
    /* Phase B bring-up will do: gpio_install_isr_service(0); and route the
       PIN_MAINS_DETECT handler to motor_drv_zero_cross_us() below. Until then
       the hook is exercised from the ZC test pulse input (bench mode). */
    ESP_LOGI(TAG, "init ok (deadtime=%dms)", DEADTIME_MS);
    return ESP_OK;
}

/* ISR-visible hook (called from GPIO isr handler once wired): */
IRAM_ATTR void motor_drv_zero_cross_us(int64_t now_us)
{
    if (s.zc_last_us) {
        int32_t hu = (int32_t)(now_us - s.zc_last_us);   /* half-period, us */
        /* Plausible band 41.7..83.3 Hz => half-period 6000..12000 us. */
        if (hu >= 6000 && hu <= 12000) {
            if (s.freq_sel == MAINS_FREQ_AUTO) {
                /* Auto-detect: median-of-8 consecutive in-band samples,
                   latched after 8 valid periods (~160 ms @50Hz). Tracking
                   the provisional mean keeps firing sane while detecting. */
                static int32_t buf[8];
                static uint8_t idx, cnt;
                buf[idx] = hu; idx = (idx + 1) & 7; if (cnt < 8) cnt++;
                int32_t sorted[8]; memcpy(sorted, buf, sizeof buf);
                for (uint8_t a = 1; a < cnt; a++)          /* insertion sort */
                    for (int32_t v = sorted[a]; a && sorted[a-1] > v; a--)
                        sorted[a] = sorted[a-1];
                int32_t med = sorted[cnt / 2];
                if (++s.meas_ok_cnt >= 8) {
                    s.meas_hz = (med > 9166) ? 50 : 60;
                    s.half_us = (uint32_t)med;   /* track actual line period */
                    idx = cnt = 0;
                } else {
                    s.half_us = (uint32_t)med;   /* provisional tracking     */
                }
            } else {
                /* Explicit regional pick: never re-decide the region, but
                   track the real measured period for firing precision.
                   Sanity: reject a measurement that contradicts the pick
                   by >5% (tripped breaker / wrong setting) -> keep nominal. */
                uint32_t nominal = half_us_for(s.freq_sel);
                int32_t  lo = (int32_t)(nominal * 95 / 100);
                int32_t  hi = (int32_t)(nominal * 105 / 100);
                if (hu >= lo && hu <= hi) s.half_us = (uint32_t)hu;
                s.meas_hz = (uint16_t)s.freq_sel;
            }
        } else {
            s.meas_ok_cnt = 0;            /* glitch -> restart detection */
        }
    }
    s.zc_last_us = now_us;
    /* Schedule firing for this half-cycle if running: */
    if (s.state != MDRV_IDLE && s.state != MDRV_FAULT && s.applied > 0) {
        /* throttle% -> conduction angle: 0% => fire at end (no power),
           100% => fire at ZC (full). delay = half*(100-p)/100, exact us. */
        int64_t delay_us = (int64_t)s.half_us * (100 - s.applied) / 100;
        s.fired_us = now_us + delay_us;
        /* one-shot timer arm omitted in skeleton (Phase B): fires TRIAC 200us */
    }
}

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
        s.energized_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        s.state = MDRV_RAMPING;
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
        uint32_t t = tp->now_ms - s.energized_ms;
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
        /* sustained-full-speed -> engage shunt relay K3 (bypass triac) */
        if (s.applied >= storage_cfg()->duty_max_pct)
            gpio_set_level(PIN_RELAY_SHUNT, 1);
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
    return !(m == 3); /* K1&K2 together is illegal */
}
