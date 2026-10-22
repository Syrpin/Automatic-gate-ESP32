#include <string.h>
#include "freertos/FreeRTOS.h"
#include "task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "encoder.h"
#include "limit_sw.h"
#include "current_sense.h"
#include "buttons.h"
#include "storage_nvs.h"
#include "events_log.h"
#include "power_mgmt.h"
#include "gate_logic.h"
#include "hal_board.h"

static const char *TAG="logic";

#define MOVE_TIMEOUT_MS 60000
#define PINCH_CURRENT_HOLD_MS 150
#define RAMP_DOWN_ZONE_PCT 15   /* begin decel within N% of limit            */

static struct {
    gate_state_t st;
    int32_t target;
    uint32_t move_start_ms;
    uint32_t pinch_since_ms;
    uint8_t beep_seq[8]; size_t beep_i;
} G;

static mdrv_state_t ms(void){ return motor_drv_state(); }

static void to(gate_state_t n,const char*why){
    if(G.st==n)return;
    ESP_LOGI(TAG,"%d->%d (%s)",G.st,n,why);
    events_log_write(EV_MOVE,"state %d->%d %s",G.st,n,why);
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
        encoder_restore_stub_set(pos); /* see note below */
        to(ok?GS_IDLE_MID:GS_CALIBRATING, ok?"restored":"needs-home");
    } else to(GS_CALIBRATING,"no trusted pos");
    return ESP_OK;
}

/* NOTE: encoder API exposes origins only at limits; mid-restore uses a private
   setter added in Phase B (documented TODO). For skeleton we re-zero there. */
extern void encoder_set_position_raw(int32_t p);
static void encoder_restore_stub_set(int32_t p){ encoder_set_position_raw(p); }

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
    G.target=storage_cfg()->travel_counts; G.move_start_ms=(uint32_t)(esp_timer_get_time()/1000);
    motor_drv_set(DIR_OPEN, storage_cfg()->duty_max_pct);
    to(GS_MOVING_OPEN,"cmd");
}
void gate_cmd_close(void){
    if(G.st==GS_FAULT) return;
    G.target=0; G.move_start_ms=(uint32_t)(esp_timer_get_time()/1000);
    motor_drv_set(DIR_CLOSE, storage_cfg()->duty_max_pct);
    to(GS_MOVING_CLOSE,"cmd");
}
void gate_cmd_stop(void){
    if(ms()==MDRV_IDLE) return;
    motor_drv_stop(true);
    to(GS_IDLE_MID,"user stop");
    storage_save_position(encoder_position(),0,true);
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
    if(moving && current_over_run(PINCH_CURRENT_HOLD_MS)){
        motor_drv_stop(false);
        to(GS_PINCH_REVERSE,"overcurrent");
        G.pinch_since_ms=tp->now_ms;
        events_log_write(EV_FAULT,"pinch I=%.2fA pos=%d",current_rms(),encoder_position());
    }
    /* encoder contradiction: energized but sensor says wrong direction */
    if(moving && !encoder_dir_matches(G.st==GS_MOVING_OPEN?DIR_OPEN:DIR_CLOSE,800)){
        motor_drv_stop(false); to(GS_FAULT,"enc contradiction");
    }
    /* no-current watchdog: running yet ~0 A => belt snapped / triac dead */
    if((ms()==MDRV_RUNNING)&&current_rms()<0.05f&&
       (uint32_t)(tp->now_ms-G.move_start_ms)>3000){
        motor_drv_stop(false); to(GS_FAULT,"no current");
    }
}

void gate_logic_step(const TickPeriod_t *tp){
    btn_event_t b; while((b=buttons_pop())!=BTN_EV_NONE) handle_btn(b);

    const limit_state_t*L=limit_sw_state();
    int32_t pos=encoder_position();
    const gate_config_t*c=storage_cfg();

    /* ramp-down before limits using speed estimate */
    if(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE){
        int32_t rem = (G.st==GS_MOVING_OPEN)? c->travel_counts-pos : pos;
        uint32_t zone=c->travel_counts*RAMP_DOWN_ZONE_PCT/100;
        if((uint32_t)rem<zone) motor_drv_set(G.st==GS_MOVING_OPEN?DIR_OPEN:DIR_CLOSE,45);
        check_pinch_or_stall(tp);
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
            gate_dir_t back=(G.target==0)?DIR_OPEN:DIR_CLOSE;
            motor_drv_set(back,c->duty_max_pct/2); G.move_start_ms=tp->now_ms;
            to(back==DIR_OPEN?GS_MOVING_OPEN:GS_MOVING_CLOSE,"pinch reverse"); }
    } else if(G.st==GS_CALIBRATING){
        /* homing: drive CLOSE at low throttle until closed limit, then zero.
           Skeleton: single-step; full FSM with both-limit learning in Phase E. */
        motor_drv_set(DIR_CLOSE,35);
        if(L->at_closed){ motor_drv_stop(true); encoder_set_origin_closed();
            to(GS_IDLE_CLOSED,"homed"); storage_save_position(0,0,true); }
    } else if(G.st==GS_FAULT){
        hal_board_safe_state_now(); /* keep outputs dead; reset via STOP x2 or app */
    }
    motor_drv_update(tp);
}

void gate_logic_beep_tick(uint32_t dt_ms){
    /* pattern engine table-driven (replaces blocking beep.c delays) */
    static const uint8_t P_MOVE[]={2,0,2,0}, P_ERR[]={1,1,1,1,1,1};
    const uint8_t*p=NULL;size_t n=0;
    if(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE){p=P_MOVE;n=4;}
    else if(G.st==GS_FAULT){p=P_ERR;n=6;}
    if(!p){gpio_beeper_off_stub();return;}
    (void)dt_ms;(void)p;(void)n; /* LEDC tone wiring: Phase B */
}
static void gpio_beeper_off_stub(void){}
static void hal_board_safe_state_now(void){ hal_board_safe_state(); }

void gate_logic_snapshot(telemetry_snapshot_t*s){
    s->position=encoder_position(); s->target=G.target; s->state=G.st;
    s->current_rma=current_rms(); s->batt_v=power_batt_volts(); s->mains_ok=power_mains_ok();
}
