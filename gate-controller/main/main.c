/*
 * main.c — gate-controller entry point (ESP32-S3 port of STM32F103 firmware)
 *
 * Replaces the old super-loop (main.c/timerClock switch) with FreeRTOS tasks:
 *   ctrl_task  @10 ms  — gate state machine, buttons, limit switches, current prot.
 *   ui_beep    @50 ms  — buzzer pattern engine (replaces blocking delays).
 *   telemetry  @1 s    — position/current/temperature/log flush.
 *   vision     (on-demand, camera_pipeline) — event-driven, not a busy task.
 *
 * All blocking while-loops from init.c/motor.c are gone by construction:
 * drivers never busy-wait; they use timers/notifications.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

#include "hal_board.h"
#include "storage_nvs.h"
#include "motor_drv.h"
#include "encoder.h"
#include "limit_sw.h"
#include "buttons.h"
#include "beep.h"
#include "current_sense.h"
#include "gate_logic.h"
#include "power_mgmt.h"
#include "events_log.h"
#include "notify_tg.h"

static const char *TAG = "main";

#define CTRL_PERIOD_MS   10
#define BEEP_PERIOD_MS   50
#define TELEMETRY_MS     1000
/* Task WDT: ctrl must finish its 10 ms budget with margin; if the state
   machine stalls (driver deadlock), reset is safer than a frozen gate. */
#define CTRL_WDT_TIMEOUT_MS  200

static void ctrl_task(void *arg)
{
    TickPeriod_t tp;
    tp.period_ms = CTRL_PERIOD_MS;

    /* WDT: subscribe AFTER registration of the timeout config; a stalled
       ctrl loop means a driver deadlock -> panic+reset (sdkconfig wdt). */
    esp_task_wdt_config_t wcfg = {
        .timeout_ms = CTRL_WDT_TIMEOUT_MS,
        .idle_core_mask = 0,          /* don't watch idle tasks here */
        .trigger_panic = true,
    };
    /* re-init is harmless if already initialised by app startup */
    (void)esp_task_wdt_init(&wcfg);
    esp_task_wdt_add(NULL);

    for (;;) {
        uint32_t t0 = esp_timer_get_time() / 1000;
        tp.now_ms = t0;

        esp_task_wdt_reset();

        buttons_sample(&tp);      /* debounce FSM, no blocking */
        limit_sw_scan(&tp);       /* de-bounce + edge events */
        current_sense_sample(&tp);/* RMS window, fault flags */
        encoder_update(&tp);      /* PCNT read + overflow-safe accumulation */
        gate_logic_step(&tp);     /* state machine -> motor_drv commands */

        uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - t0;
        int32_t remain = CTRL_PERIOD_MS - (int32_t)elapsed;
        vTaskDelay(pdMS_TO_TICKS(remain > 2 ? remain : 2));
    }
}

static void beep_task(void *arg)
{
    (void)arg;
    for (;;) {
        gate_logic_beep_tick(BEEP_PERIOD_MS);
        vTaskDelay(pdMS_TO_TICKS(BEEP_PERIOD_MS));
    }
}

static void telemetry_task(void *arg)
{
    (void)arg;
    TickPeriod_t tp; tp.period_ms = TELEMETRY_MS;
    for (;;) {
        tp.now_ms = (uint32_t)(esp_timer_get_time()/1000);
        telemetry_snapshot_t s;
        gate_logic_snapshot(&s);
        ESP_LOGI(TAG, "pos=%d tgt=%d state=%d I_rms=%.2fA batt=%.2fV mains=%d",
                 s.position, s.target, (int)s.state, s.current_rma,
                 s.batt_v, s.mains_ok);
        events_log_flush();          /* batched JSONL write to 'log' partition */
        power_mgmt_heartbeat();      /* feed task WDT + battery thermal check */
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_MS));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(hal_board_init());      /* clocks, GPIO defaults safe-state */
    /* One shared GPIO ISR service for encoder (quadrature) + motor_drv (ZC).
       Must be installed BEFORE any gpio_isr_handler_add() call. */
    ESP_ERROR_CHECK(hal_board_isr_service_install());
    ESP_ERROR_CHECK(storage_nvs_init());    /* config + persisted position */
    events_log_init();                      /* ring buffer + journal loader */

    ESP_ERROR_CHECK(motor_drv_init());      /* MCPWM phase-angle + relay seq/dead-time */
    ESP_ERROR_CHECK(encoder_init());        /* PCNT channel per profile (optical/DX/DI) */
    ESP_ERROR_CHECK(limit_sw_init());
    ESP_ERROR_CHECK(buttons_init());
    ESP_ERROR_CHECK(beep_init());           /* LEDC tone engine (non-blocking patterns) */
    ESP_ERROR_CHECK(current_sense_init());  /* ADC1 + DMA, zero-cross reference */
    ESP_ERROR_CHECK(power_mgmt_init());     /* mains detect, UPS budget, graceful save */

    ESP_ERROR_CHECK(gate_logic_init());     /* restores position, re-validates limits */

    /* Telegram notifications: queue-backed, never blocks ctrl_task; Wi-Fi
       bring-up happens inside notify_tg (Phase C stub until then). */
    if (notify_tg_init() != ESP_OK)
        ESP_LOGW(TAG, "notify_tg init deferred (no network yet)");

    /* Phase C stub: camera_pipeline_init()/vision_core wired in next stage. */

    xTaskCreatePinnedToCore(ctrl_task, "ctrl", 4096, NULL, 6, NULL, 0);
    xTaskCreate(beep_task, "beep", 2048, NULL, 4, NULL);
    xTaskCreate(telemetry_task, "telem", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "gate-controller boot complete (heap=%lu)",
             (unsigned long)xPortGetFreeHeapSize());
}
