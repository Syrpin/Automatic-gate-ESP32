/*
 * sim.c — virtual peripherals + plant model + USB-UART REPL (CONFIG_GATE_SIM).
 *
 * The production logic stack runs UNCHANGED; only physical I/O leaves are
 * redirected. Drivers consult these hooks guarded by #ifdef CONFIG_GATE_SIM
 * (see buttons.c, limit_sw.c, current_sense.c, power_mgmt.c) — shipping
 * builds contain no test code paths.
 *
 * Transport: on the S3 devkit the single micro-USB cable is USB-Serial-JTAG;
 * with CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y the REPL speaks CDC over it
 * (`idf.py monitor` or any terminal). Classic UART0 works too.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_check.h"

#include "sim.h"
#include "sim_hooks.h"
#include "storage_nvs.h"
#include "motor_drv.h"
#include "encoder.h"
#include "limit_sw.h"
#include "power_mgmt.h"
#include "gate_logic.h"
#include "events_log.h"

static const char *TAG = "sim";
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

/* ---------------- virtual peripheral state ---------------- */
static volatile bool    s_btn_held[3];        /* raw button lines            */
static volatile bool    s_limit[2];           /* [0]=CLOSED sw [1]=OPEN sw   */
static volatile bool    s_mains_ok = true;
static volatile float   s_inj_current = -1.f; /* amps; <0 => injection off   */
static volatile int32_t s_enc_pending;        /* counts queued for injection */
static volatile float   s_plant_amps;         /* modeled load current        */
static bool             s_plant_on = true;
static float            s_plant_speed_cps;    /* modeled gate speed          */

/* ---------------- hook implementations (consumed by drivers in sim_hooks.h) */
bool  sim_button_active(void)   { return true; }
int   sim_button_level(int idx) { return (idx >= 0 && idx < 3 && s_btn_held[idx]) ? 0 : 1; }
bool  sim_limit_active(void)    { return true; }
int   sim_limit_level(int id)   { return (id == 0 || id == 1) ? (s_limit[id] ? 0 : 1) : 1; }
bool  sim_mains_state(bool *ok_out) { *ok_out = s_mains_ok; return true; }

/* CT pipeline: injected value wins, else plant-model amps feed the window. */
bool  sim_current_value(float *amps_out)
{
    if (s_inj_current >= 0.f) { *amps_out = s_inj_current; return true; }
    *amps_out = s_plant_amps;
    return s_plant_amps > 0.f;
}

/* ---------------- injection API --------------------------- */
typedef struct { int idx; uint32_t ms; } rel_arg_t;
static void btn_release_task(void *arg)
{
    rel_arg_t *a = (rel_arg_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(a->ms));
    if (a->idx >= 0 && a->idx < 3) s_btn_held[a->idx] = false;
    free(a);
    vTaskDelete(NULL);
}

void sim_btn_press(int idx, uint32_t ms)
{
    if (idx < 0 || idx > 2) return;
    s_btn_held[idx] = true;
    rel_arg_t *a = malloc(sizeof *a);
    if (!a) { s_btn_held[idx] = false; return; }
    a->idx = idx; a->ms = ms ? ms : 150;
    xTaskCreate(btn_release_task, "btn_rel", 1536, a, 5, NULL);
}

void sim_limit_set(int id, bool engaged)
{
    if (id == 0 || id == 1) {
        s_limit[id] = engaged;
        ESP_LOGI(TAG, "limit %s := %d", id ? "OPEN" : "CLOSED", (int)engaged);
    }
}

void sim_mains_set(bool ok)
{
    if (s_mains_ok == ok) return;
    s_mains_ok = ok;
    events_log_write(EV_POWER, "SIM mains %s", ok ? "restored" : "LOST");
}

void sim_enc_add(int32_t d) { __atomic_fetch_add(&s_enc_pending, d, __ATOMIC_RELAXED); }
void sim_current_inject(float amps)   { s_inj_current = amps; }
bool sim_current_override_active(void){ return s_inj_current >= 0.f; }
void sim_plant_model_enable(bool on)  { s_plant_on = on; }

/* ---------------- plant model: drive -> counts + current --- */
#define PLANT_MAX_CPS 60.0f   /* default travel 4000 -> full cycle ~66 s */

void sim_plant_step(uint32_t dt_ms)
{
    if (!s_plant_on) return;
    mdrv_state_t st = motor_drv_state();
    gate_dir_t dir  = motor_drv_direction();

    float cps = 0.f, amps = 0.f;
    if (dir != DIR_NONE && (st == MDRV_RAMPING || st == MDRV_RUNNING || st == MDRV_DECEL)) {
        float target = PLANT_MAX_CPS * (dir == DIR_OPEN ? 1.f : -1.f);
        if (st == MDRV_DECEL) target *= 0.3f;
        s_plant_speed_cps += 0.15f * (target - s_plant_speed_cps);
        cps = s_plant_speed_cps;
        amps = 1.2f + 0.03f * fabsf(cps);
        if (st == MDRV_RAMPING) amps += 2.5f;     /* inrush-like start */
    } else {
        s_plant_speed_cps = 0.f;
    }
    s_plant_amps = amps;

    int32_t d = (int32_t)(cps * dt_ms / 1000.f);
    if (d) sim_enc_add(d);

    /* emulate mechanical stops at both ends of travel */
    int32_t pos = encoder_position();
    const gate_config_t *c = storage_cfg();
    if (pos <= 0)                s_limit[0] = (cps <= 0.05f);
    else                         s_limit[0] = false;
    if (pos >= c->travel_counts) s_limit[1] = (cps >= -0.05f);
    else                         s_limit[1] = false;
}

/* ---------------- JSON snapshot (web UI shares this) ------- */
static const char *state_name(gate_state_t s)
{
    switch (s) {
    case GS_IDLE_CLOSED:  return "IDLE_CLOSED";
    case GS_IDLE_OPEN:    return "IDLE_OPEN";
    case GS_IDLE_MID:     return "IDLE_MID";
    case GS_MOVING_OPEN:  return "MOVING_OPEN";
    case GS_MOVING_CLOSE: return "MOVING_CLOSE";
    case GS_PINCH_REVERSE:return "PINCH_REVERSE";
    case GS_CALIBRATING:  return "CALIBRATING";
    default:              return "FAULT";
    }
}

int sim_status_json(char *buf, int cap)
{
    telemetry_snapshot_t s;
    gate_logic_snapshot(&s);
    uint8_t relays = 0; bool rel_ok = motor_drv_relay_health(&relays);
    const limit_state_t *L = limit_sw_state();
    return snprintf(buf, cap,
        "{\"pos\":%d,\"target\":%d,\"travel\":%d,"
        "\"state\":\"%s\",\"mdrv\":%d,\"dir\":%d,"
        "\"i_rms\":%.2f,\"inj\":%d,"
        "\"batt_v\":%.2f,\"batt_pct\":%u,\"mains\":%s,"
        "\"lim_closed\":%s,\"lim_open\":%s,\"lim_fault\":%s,"
        "\"relays\":%u,\"relays_ok\":%s,\"enc_status\":%d,"
        "\"ac_sel\":%d,\"ac_meas\":%u,\"ac_half_us\":%u,\"zc_sim\":%s,"
        "\"log_dropped\":%u}",
        s.position, s.target, (int)storage_cfg()->travel_counts,
        state_name(s.state), (int)motor_drv_state(), (int)motor_drv_direction(),
        s.current_rma, (int)sim_current_override_active(),
        s.batt_v, (unsigned)power_battery_pct(), s.mains_ok ? "true" : "false",
        L->at_closed ? "true" : "false", L->at_open ? "true" : "false",
        L->wire_fault ? "true" : "false",
        (unsigned)relays, rel_ok ? "true" : "false",
        (int)encoder_status(),
        (int)motor_drv_mains_freq(), (unsigned)motor_drv_measured_hz(),
        (unsigned)motor_drv_half_period_us(),
        sim_zc_running() ? "true" : "false",
        (unsigned)events_log_dropped());
}

/* ---------------- console commands ------------------------- */

/* Virtual mains zero-cross generator: feeds motor_drv_zero_cross_us() at the
 * selected line frequency so phase-angle timing is testable on a bare board. */
static TaskHandle_t s_zc_task;
static volatile uint16_t s_zc_hz;

static void zc_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (s_zc_hz == 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        /* Two zero-crosses per mains period (both half-cycles fire the triac),
           so the ZC event rate is 2*hz, not hz. esp_timer gives us sub-ms
           accuracy that vTaskDelay(1 tick @1ms) cannot: schedule the next
           crossing on an absolute timeline to avoid tick-rounding drift
           (10 ms -> 10 ticks vs 8.333 ms -> 8/9 alternating). */
        int64_t t = esp_timer_get_time();
        motor_drv_zero_cross_us(t);
        int64_t half_us = 500000 / s_zc_hz;          /* exact: 10000/8333 us */
        int64_t wake = t + half_us;
        int64_t now = esp_timer_get_time();
        int64_t sleep_us = wake - now;
        if (sleep_us > 1500) {                       /* coarse part via RTOS */
            vTaskDelay(pdMS_TO_TICKS((uint32_t)((sleep_us - 1000) / 1000)));
        }
        while (esp_timer_get_time() < wake) { /* busy-wait final <=1.5 ms */ }
    }
}

void sim_zc_start(uint16_t hz)
{
    if (hz != 50 && hz != 60) return;
    s_zc_hz = hz;
    if (!s_zc_task) xTaskCreate(zc_task, "sim_zc", 2048, NULL, 7, &s_zc_task);
    ESP_LOGI(TAG, "virtual mains ZC := %u Hz", (unsigned)hz);
}
void sim_zc_stop(void) { s_zc_hz = 0; }
bool sim_zc_running(void) { return s_zc_hz != 0; }

static int cmd_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    char buf[768];
    sim_status_json(buf, sizeof buf);
    printf("%s\n", buf);
    return 0;
}

static int cmd_btn(int argc, char **argv)
{
    if (argc < 2) { printf("usage: btn open|stop|close [ms]\n"); return 1; }
    int idx = !strcmp(argv[1], "open") ? 0 : !strcmp(argv[1], "stop") ? 1 :
              !strcmp(argv[1], "close") ? 2 : -1;
    if (idx < 0) { printf("bad button '%s'\n", argv[1]); return 1; }
    uint32_t ms = argc > 2 ? (uint32_t)atoi(argv[2]) : 150;
    sim_btn_press(idx, ms);
    printf("pressed %s for %ums\n", argv[1], (unsigned)ms);
    return 0;
}

static int cmd_limit(int argc, char **argv)
{
    if (argc < 3) { printf("usage: limit closed|open 0|1\n"); return 1; }
    sim_limit_set(!strcmp(argv[1], "closed") ? 0 : 1, atoi(argv[2]) != 0);
    return 0;
}

static int cmd_enc(int argc, char **argv)
{
    if (argc < 2) { printf("usage: enc <delta> | enc set <abs>\n"); return 1; }
    if (!strcmp(argv[1], "set") && argc > 2) {
        int32_t abs = atoi(argv[2]);
        int32_t d = abs - encoder_position();
        sim_enc_add(d);
        printf("seek %d (delta %+d)\n", (int)abs, (int)d);
    } else {
        sim_enc_add(atoi(argv[1]));
    }
    return 0;
}

static int cmd_i(int argc, char **argv)
{
    if (argc < 2) { printf("usage: i <amps> | i off\n"); return 1; }
    if (!strcmp(argv[1], "off")) { sim_current_inject(-1.f); printf("inject off (plant model resumes)\n"); }
    else { sim_current_inject(strtof(argv[1], NULL)); printf("injecting %s A\n", argv[1]); }
    return 0;
}

static int cmd_mains(int argc, char **argv)
{
    if (argc < 2) { printf("usage: mains on|off\n"); return 1; }
    sim_mains_set(!strcmp(argv[1], "on"));
    printf("mains := %s\n", argv[1]);
    return 0;
}

static int cmd_plant(int argc, char **argv)
{
    if (argc < 2) { printf("usage: plant on|off\n"); return 1; }
    sim_plant_model_enable(!strcmp(argv[1], "on"));
    printf("plant model := %s\n", argv[1]);
    return 0;
}

/* Regional / mains-frequency console surface. */
static int cmd_ac(int argc, char **argv)
{
    if (argc < 2) {
        printf("ac sel=%s meas=%uHz half=%uus zc_sim=%s\n",
               motor_drv_mains_freq() == MAINS_FREQ_50HZ ? "50" :
               motor_drv_mains_freq() == MAINS_FREQ_60HZ ? "60" : "AUTO",
               (unsigned)motor_drv_measured_hz(),
               (unsigned)motor_drv_half_period_us(),
               sim_zc_running() ? "on" : "off");
        return 0;
    }
    if (!strcmp(argv[1], "auto") || !strcmp(argv[1], "0")) {
        motor_drv_set_mains_freq(MAINS_FREQ_AUTO);
    } else if (!strcmp(argv[1], "50")) {
        motor_drv_set_mains_freq(MAINS_FREQ_50HZ);
    } else if (!strcmp(argv[1], "60")) {
        motor_drv_set_mains_freq(MAINS_FREQ_60HZ);
    } else if (!strcmp(argv[1], "zc")) {          /* virtual mains generator */
        uint16_t hz = argc > 2 ? (uint16_t)atoi(argv[2]) : 50;
        if (hz == 0) sim_zc_stop(); else sim_zc_start(hz);
    } else { printf("usage: ac auto|50|60 | ac zc 50|60|0(stop)\n"); return 1; }
    return 0;
}

static int cmd_cfg(int argc, char **argv)
{
    /* Persist a regional pick into NVS config (survives reboot). */
    if (argc < 3) { printf("usage: cfg mains_freq auto|50|60\n"); return 1; }
    gate_config_t c = *storage_cfg();
    if      (!strcmp(argv[2], "auto")) c.mains_freq = MAINS_FREQ_AUTO;
    else if (!strcmp(argv[2], "50"))   { c.mains_freq = MAINS_FREQ_50HZ; c.region_code = 1; }
    else if (!strcmp(argv[2], "60"))   { c.mains_freq = MAINS_FREQ_60HZ; c.region_code = 2; }
    else { printf("bad value\n"); return 1; }
    esp_err_t e = storage_cfg_commit(&c);
    if (e != ESP_OK) { printf("commit failed (%s)\n", esp_err_to_name(e)); return 1; }
    motor_drv_set_mains_freq(c.mains_freq);
    events_log_write(EV_POWER, "cfg mains_freq := %s", argv[2]);
    printf("saved: mains_freq=%s region=%u\n", argv[2], (unsigned)c.region_code);
    return 0;
}

static int cmd_fault(int argc, char **argv)
{
    if (argc < 2) { printf("usage: fault both-limits|stall|nocurrent\n"); return 1; }
    if (!strcmp(argv[1], "both-limits")) {
        sim_limit_set(0, true); sim_limit_set(1, true);
        printf("drill: both limits engaged -> wire_fault expected during next move\n");
    } else if (!strcmp(argv[1], "stall")) {
        sim_current_inject(storage_cfg()->i_rms_run_max + 2.f);
        printf("drill: overcurrent injected -> pinch fires ~150ms into motion\n");
    } else if (!strcmp(argv[1], "nocurrent")) {
        sim_current_inject(0.01f);
        printf("drill: near-zero current while running -> watchdog after 3s\n");
    } else return 1;
    return 0;
}

static int cmd_reset(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("rebooting...\n"); fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return 0;
}

static int cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf(
"\nGate simulator console (bare-board HIL)\n"
"  status                     JSON snapshot (same feed as web UI)\n"
"  btn open|stop|close [ms]   virtual button press\n"
"  limit closed|open 0|1      engage/release end switch\n"
"  enc <delta> | enc set <n>  inject position counts\n"
"  i <amps> | i off           override CT current (pinch/stall drills)\n"
"  mains on|off               simulate loss/restore of mains\n"
"  plant on|off               motor->encoder plant model\n"
"  ac [auto|50|60]            regional mains freq (show/set)\n"
"  ac zc 50|60|0              virtual zero-cross generator (bare-board test)\n"
"  cfg mains_freq auto|50|60  persist regional pick to NVS\n"
"  fault both-limits|stall|nocurrent   canned safety drills\n"
"  reset                      reboot device\n"
"Web UI: http://<device-ip>/ when Wi-Fi is up - mirrors all controls.\n");
    return 0;
}

/* ---------------- engine task: plant + encoder injection drain */
static void sim_engine_task(void *arg)
{
    (void)arg;
    for (;;) {
        sim_plant_step(10);
        int32_t d;
        taskENTER_CRITICAL(&s_mux, NULL);
        d = s_enc_pending; s_enc_pending = 0;
        taskEXIT_CRITICAL(&s_mux, NULL);
        if (d) encoder_add_sim_counts(d);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t sim_start(void)
{
    xTaskCreate(sim_engine_task, "sim_eng", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "simulator engine started (plant=%s)", s_plant_on ? "on" : "off");
    return ESP_OK;
}

esp_err_t sim_console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "gate>";
#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t dc = ESP_CONSOLE_DEV_USJ_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_usj(&dc, &rc, &repl), TAG, "usj repl");
#else
    esp_console_dev_uart_config_t dc = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&dc, &rc, &repl), TAG, "uart repl");
#endif
    const esp_console_cmd_t cmds[] = {
        { .command = "status", .help = "JSON state",           .func = cmd_status },
        { .command = "btn",    .help = "virtual button press", .func = cmd_btn },
        { .command = "limit",  .help = "end-switch control",   .func = cmd_limit },
        { .command = "enc",    .help = "inject counts",        .func = cmd_enc },
        { .command = "i",      .help = "current injection",    .func = cmd_i },
        { .command = "mains",  .help = "simulate mains",       .func = cmd_mains },
        { .command = "plant",  .help = "plant model on/off",   .func = cmd_plant },
        { .command = "ac",     .help = "regional mains freq / ZC sim", .func = cmd_ac },
        { .command = "cfg",    .help = "persist config to NVS", .func = cmd_cfg },
        { .command = "fault",  .help = "safety drills",        .func = cmd_fault },
        { .command = "reset",  .help = "reboot",               .func = cmd_reset },
        { .command = "help",   .help = "this text",            .func = cmd_help },
    };
    for (size_t i = 0; i < sizeof cmds / sizeof cmds[0]; i++)
        ESP_RETURN_ON_ERROR(esp_console_register_command(&cmds[i]), TAG, "cmd %s", cmds[i].command);
    ESP_RETURN_ON_ERROR(esp_console_start_repl(repl), TAG, "start repl");
    cmd_help(0, NULL);
    return ESP_OK;
}
