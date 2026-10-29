#include <string.h>
#include <math.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "encoder.h"
#include "limit_sw.h"
#include "current_sense.h"
#include "buttons.h"
#include "beep.h"
#include "storage_nvs.h"
#include "events_log.h"
#include "power_mgmt.h"
#include "gate_logic.h"
#include "hal_board.h"

static const char *TAG="logic";

#define MOVE_TIMEOUT_MS 60000
#define PINCH_CURRENT_HOLD_MS 150
#define RAMP_DOWN_ZONE_PCT 15   /* begin decel within N% of limit            */
#define BACK_OFF_MM        300  /* mechanical back-off after pinch           */
#define MM_TO_COUNTS       1    /* counts per mm: wizard-calibrated (Phase E)*/
#define PINCH_MAX_ATTEMPTS 2    /* then latch FAULT instead of double-back   */
#define NO_CURRENT_AMP     0.05f /* CT deadband; meaningless until gain!=0   */
#define NO_CURRENT_MIN_MS  3000  /* grace before no-current watchdog arms    */

static struct {
    gate_state_t st;
    int32_t target;
    int32_t back_off_to;             /* stop point for the reverse move      */
    gate_dir_t pending_dir;          /* GS_RETRY_START deferred command      */
    uint8_t pinch_attempts;
    uint32_t move_start_ms;
    uint32_t pinch_since_ms;
    uint32_t energized_no_cur_since; /* 0 = disarmed                          */
    uint8_t beep_seq[8]; size_t beep_i;
} G;

static mdrv_state_t ms(void){ return motor_drv_state(); }
static void hal_board_safe_state_now(void);
static uint16_t c_inrush_window(void){ return storage_cfg()->inrush_window_ms; }
static bool current_over_peak_abs(void){ return current_rms()>storage_cfg()->i_rms_peak_max; }

static void to(gate_state_t n,const char*why){
    if(G.st==n)return;
    ESP_LOGI(TAG,"%d->%d (%s)",G.st,n,why);
    events_log_write(EV_MOVE,"state %d->%d %s",G.st,n,why);
    /* one-shot audio feedback on the transitions that matter to the user */
    if(n==GS_FAULT) beep_error();
    G.st=n;
}

esp_err_t gate_logic_init(void){
    memset(&G,0,sizeof G);
    int32_t pos; uint32_t epoch; bool trusted;
    const limit_state_t*L=limit_sw_state();
    if(storage_load_position(&pos,&epoch,&trusted)==ESP_OK && trusted){
        /* triple validation on boot: persisted pos must agree with any engaged
           limit switch, else force re-homing (old code just trusted blindly) */
        bool ok = (!L->at_closed || pos<=storage_cfg()->enc_min) &&
                  (!L->at_open  || pos>=storage_cfg()->travel_counts-storage_cfg()->enc_min);
        encoder_set_position_raw(pos);
        to(ok?GS_IDLE_MID:GS_CALIBRATING, ok?"restored":"needs-home");
    } else to(GS_CALIBRATING,"no trusted pos");
    return ESP_OK;
}
static void handle_btn(btn_event_t e){
    switch(e){
    case BTN_STOP_SHORT:  gate_cmd_stop(); break;
    case BTN_OPEN_SHORT:  gate_cmd_open(); break;
    case BTN_CLOSE_SHORT: gate_cmd_close(); break;
    case BTN_OPEN_LONG:   /* hold-to-move guard could require long press cfg flag */
        gate_cmd_open(); break;
    case BTN_CLOSE_LONG:  gate_cmd_close(); break;
    default: break;
    }
}

void gate_cmd_open(void){
    if(G.st==GS_FAULT) return;
    G.pinch_attempts=0;              /* new user/vision cmd resets back-off policy */
    G.target=storage_cfg()->travel_counts; G.move_start_ms=(uint32_t)(esp_timer_get_time()/1000);
    if(motor_drv_set(DIR_OPEN, storage_cfg()->duty_max_pct)!=ESP_OK){
        /* reversal rejected by dead-time FSM: retry from stop on next tick */
        G.pending_dir=DIR_OPEN; to(GS_RETRY_START,"drv reject"); return; }
    to(GS_MOVING_OPEN,"cmd");
}
void gate_cmd_close(void){
    if(G.st==GS_FAULT) return;
    G.pinch_attempts=0;
    G.target=0; G.move_start_ms=(uint32_t)(esp_timer_get_time()/1000);
    if(motor_drv_set(DIR_CLOSE, storage_cfg()->duty_max_pct)!=ESP_OK){
        G.pending_dir=DIR_CLOSE; to(GS_RETRY_START,"drv reject"); return; }
    to(GS_MOVING_CLOSE,"cmd");
}
void gate_cmd_stop(void){
    if(ms()==MDRV_IDLE && G.st!=GS_MOVING_OPEN && G.st!=GS_MOVING_CLOSE &&
       G.st!=GS_PINCH_REVERSE && G.st!=GS_RETRY_START) return;
    motor_drv_stop(false);           /* user stop is HARD: no coasting */
    G.pending_dir=DIR_NONE;
    int32_t pos=encoder_position();
    const limit_state_t*L=limit_sw_state();
    to(L->at_closed?GS_IDLE_CLOSED: L->at_open?GS_IDLE_OPEN:GS_IDLE_MID,"user stop");
    storage_save_position(pos,0,true);
}

void gate_vision_event(int kind,int vid){
    /* kinds: 1=APPROACH_OK (open), 2=INSIDE (hold), 3=LEFT_CLEAR (auto-close) */
    (void)vid;
    if(kind==1 && (G.st==GS_IDLE_CLOSED||G.st==GS_IDLE_MID)) gate_cmd_open();
    else if(kind==3 && G.st==GS_IDLE_OPEN) gate_cmd_close();
    /* kind==2 extends auto-close timer via power-less timer var (Phase D) */
}

static void check_pinch_or_stall(const TickPeriod_t*tp){
    if(!storage_cfg()->pinch_detect_en) return;
    bool moving=(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE);
    /* inrush allowance: during first cfg->inrush_window_ms of a move only the
       peak threshold trips (old code blocked on startup current or ignored it) */
    bool in_inrush=(uint32_t)(tp->now_ms-G.move_start_ms)<c_inrush_window();
    if(moving && current_over_run(PINCH_CURRENT_HOLD_MS)){
        if(in_inrush && !current_over_peak_abs()){ return; } /* normal start spike */
        motor_drv_stop(false);
        to(GS_PINCH_REVERSE,"overcurrent");
        G.pinch_since_ms=tp->now_ms;
        events_log_write(EV_FAULT,"pinch I=%.2fA pos=%d",current_rms(),encoder_position());
    }
    /* encoder contradiction: energized but sensor says wrong direction */
    if(moving && !encoder_dir_matches(G.st==GS_MOVING_OPEN?DIR_OPEN:DIR_CLOSE,800)){
        motor_drv_stop(false); to(GS_FAULT,"enc contradiction");
    }
    /* no-current watchdog: energized yet ~0 A => belt snapped / triac dead /
       CT wire broken. Armed ONLY when the CT gain is calibrated (>0): with
       gain==0 current_rms() is structurally 0 and would trip instantly —
       fail-safe decision (closes README TODO #2 by construction). Own timer
       anchored at drive energization so mid-move re-issues cannot reset it. */
    if(ms()!=MDRV_IDLE && ms()!=MDRV_FAULT){
        if(!G.energized_no_cur_since) G.energized_no_cur_since=tp->now_ms;
        bool armed = storage_cfg()->ct_gain_a_per_lsb > 0.0f;
        if(armed && current_rms()<NO_CURRENT_AMP &&
           (uint32_t)(tp->now_ms-G.energized_no_cur_since)>NO_CURRENT_MIN_MS){
            motor_drv_stop(false); G.energized_no_cur_since=0;
            to(GS_FAULT,"no current");
        }
    } else G.energized_no_cur_since=0;
}

void gate_logic_step(const TickPeriod_t *tp){
    btn_event_t b; while((b=buttons_pop())!=BTN_EV_NONE) handle_btn(b);

    /* mains-loss watchdog on the fast 10 ms path (plan p.9.1 step 1 budget);
       heartbeat also calls it, but ctrl tick gives <35 ms detection */
    power_mains_watchdog();

    const limit_state_t*L=limit_sw_state();
    int32_t pos=encoder_position();
    const gate_config_t*c=storage_cfg();

    /* ramp-down before limits using speed estimate */
    if(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE){
        int32_t rem = (G.st==GS_MOVING_OPEN)? c->travel_counts-pos : pos;
        uint32_t zone=c->travel_counts*RAMP_DOWN_ZONE_PCT/100;
        if((uint32_t)rem<zone) motor_drv_set(G.st==GS_MOVING_OPEN?DIR_OPEN:DIR_CLOSE,45);
        check_pinch_or_stall(tp);
        /* back-off moves stop at their computed point, NOT at a limit switch */
        if(G.back_off_to!=INT32_MIN &&
           ((G.st==GS_MOVING_OPEN && pos>=G.back_off_to)||
            (G.st==GS_MOVING_CLOSE&& pos<=G.back_off_to))){
            motor_drv_stop(true); G.back_off_to=INT32_MIN;
            to(GS_IDLE_MID,"back-off done"); storage_save_position(pos,0,true);
        }
        /* hard limit arrival -> settle & persist */
        if(L->wire_fault){ motor_drv_stop(false); to(GS_FAULT,"limit wire fault"); }
        if(G.st==GS_MOVING_CLOSE && L->at_closed){ motor_drv_stop(true); encoder_set_origin_closed();
            to(GS_IDLE_CLOSED,"closed"); storage_save_position(0,0,true);}
        if(G.st==GS_MOVING_OPEN && L->at_open){ motor_drv_stop(true); encoder_set_origin_open();
            to(GS_IDLE_OPEN,"opened"); storage_save_position(c->travel_counts,0,true);}
        if((uint32_t)(tp->now_ms-G.move_start_ms)>MOVE_TIMEOUT_MS){
            motor_drv_stop(false); to(GS_FAULT,"move timeout");}
    } else if(G.st==GS_PINCH_REVERSE){
        if((uint32_t)(tp->now_ms-G.pinch_since_ms)>700){ /* pause then back off */
            if(++G.pinch_attempts>PINCH_MAX_ATTEMPTS){
                /* agreed policy: two failed attempts latch FAULT instead of
                   double-back-off forever (user reset via STOP/app required) */
                motor_drv_stop(false); to(GS_FAULT,"pinch attempts exhausted");
            } else {
                gate_dir_t back=(G.target==0)?DIR_OPEN:DIR_CLOSE;
                int32_t p0=encoder_position();
                int32_t off=BACK_OFF_MM*MM_TO_COUNTS;
                G.back_off_to=(back==DIR_OPEN)? p0+off : p0-off;
                if(motor_drv_set(back,c->duty_max_pct/2)!=ESP_OK){
                    G.pending_dir=back; to(GS_RETRY_START,"reverse reject"); }
                else { G.move_start_ms=tp->now_ms;
                       to(back==DIR_OPEN?GS_MOVING_OPEN:GS_MOVING_CLOSE,"pinch reverse"); }
            }
        }
    } else if(G.st==GS_RETRY_START){
        /* wait for drv FSM to reach IDLE (stop+dead-time), then issue once */
        if(ms()==MDRV_IDLE && G.pending_dir!=DIR_NONE){
            gate_dir_t d=G.pending_dir; G.pending_dir=DIR_NONE;
            if(motor_drv_set(d,c->duty_max_pct)==ESP_OK)
                to(d==DIR_OPEN?GS_MOVING_OPEN:GS_MOVING_CLOSE,"retry start");
            else to(GS_FAULT,"drv won't start");
        } else if((uint32_t)(tp->now_ms-G.move_start_ms)>3000){
            motor_drv_stop(false); to(GS_FAULT,"retry timeout"); }
    } else if(G.st==GS_CALIBRATING){
        /* homing: drive CLOSE at low throttle until closed limit, then zero.
           Skeleton: single-step; full FSM with both-limit learning in Phase E. */
        if(ms()==MDRV_IDLE) motor_drv_set(DIR_CLOSE,35);
        if(L->at_closed){ motor_drv_stop(true); encoder_set_origin_closed();
            to(GS_IDLE_CLOSED,"homed"); storage_save_position(0,0,true); }
    } else if(G.st==GS_FAULT){
        hal_board_safe_state_now(); /* keep outputs dead; reset via STOP x2 or app */
    }
    motor_drv_update(tp);
}

void gate_logic_beep_tick(uint32_t dt_ms){
    /* State -> looping pattern mapping; one-shot patterns (confirm/warn/error)
       are fired from the transitions themselves. beep_update() advances the
       table-driven engine — never blocks, never busy-waits. */
    if(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE||G.st==GS_RETRY_START)
        beep_move();
    else if(G.st==GS_FAULT)
        beep_fault_loop();
    else
        beep_stop_pattern();
    beep_update(dt_ms);
}
static void hal_board_safe_state_now(void){ hal_board_safe_state(); }

void gate_logic_snapshot(telemetry_snapshot_t*s){
    s->position=encoder_position(); s->target=G.target; s->state=G.st;
    s->current_rma=current_rms(); s->batt_v=power_batt_volts(); s->mains_ok=power_mains_ok();
}
