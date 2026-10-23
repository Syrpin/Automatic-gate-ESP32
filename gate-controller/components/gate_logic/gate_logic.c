#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#define HOMING_TIMEOUT_MS 60000
#define DIRECTION_CHANGE_DEAD_TIME_MS 250
#define DIRECTION_STOP_TIMEOUT_MS 3000
#define PINCH_CURRENT_HOLD_MS 150
#define RAMP_DOWN_ZONE_PCT 15   /* begin decel within N% of limit            */

static struct {
    gate_state_t st;
    int32_t target;
    uint32_t move_start_ms;
    uint32_t homing_start_ms;
    uint32_t pinch_since_ms;
    uint32_t direction_stop_ms;
    uint32_t restart_deadline_ms;
    gate_dir_t last_dir, pending_dir;
    gate_state_t pending_state;
    uint8_t pending_throttle;
    bool restart_pending, restart_deadtime_started, homing_command_started;
    uint8_t beep_seq[8]; size_t beep_i;
} G;

static mdrv_state_t ms(void){ return motor_drv_state(); }
static void encoder_restore_stub_set(int32_t p);
static void hal_board_safe_state_now(void);
static void gpio_beeper_off_stub(void);
static void latch_fault(const char *why, esp_err_t command_error);

static void to(gate_state_t n,const char*why){
    if(G.st==n)return;
    ESP_LOGI(TAG,"%d->%d (%s)",G.st,n,why);
    events_log_write(EV_MOVE,"state %d->%d %s",G.st,n,why);
    G.st=n;
}

static uint32_t now_ms(void){
    return (uint32_t)(esp_timer_get_time()/1000);
}

static void latch_fault(const char *why, esp_err_t command_error){
    G.restart_pending=false;
    G.homing_command_started=false;
    motor_drv_stop(false);
    if(command_error!=ESP_OK)
        events_log_write(EV_FAULT,"motor command failed (%s): %s",why,
                         esp_err_to_name(command_error));
    else
        events_log_write(EV_FAULT,"%s",why);
    to(GS_FAULT,why);
}

static bool set_motor_command(gate_dir_t dir,uint8_t throttle,
                              gate_state_t state,const char *reason){
    esp_err_t err=motor_drv_set(dir,throttle);
    if(err!=ESP_OK){ latch_fault(reason,err); return false; }
    G.last_dir=dir;
    G.move_start_ms=now_ms();
    to(state,reason);
    return true;
}

static void request_move(gate_dir_t dir,int32_t target,uint8_t throttle,
                         gate_state_t state,const char *reason){
    if(G.st==GS_FAULT) return;
    if(G.st==GS_CALIBRATING){
        ESP_LOGW(TAG,"ignoring motion command during homing");
        return;
    }
    if(motor_drv_state()==MDRV_FAULT){
        latch_fault("motor driver fault before command",ESP_OK);
        return;
    }
    G.target=target;

    if(G.restart_pending){
        if(G.pending_dir!=dir) G.restart_deadtime_started=false;
        G.pending_dir=dir;
        G.pending_state=state;
        G.pending_throttle=throttle;
        return;
    }

    gate_dir_t active_dir=motor_drv_direction();
    bool stopping=ms()==MDRV_DECEL;
    bool reversing=(G.last_dir!=DIR_NONE && G.last_dir!=dir) ||
                   (active_dir!=DIR_NONE && active_dir!=dir) ||
                   (G.st==GS_MOVING_OPEN && dir==DIR_CLOSE) ||
                   (G.st==GS_MOVING_CLOSE && dir==DIR_OPEN);
    if(!reversing && G.st==state && active_dir==dir && ms()!=MDRV_IDLE)
        return;
    if(reversing||stopping){
        G.restart_pending=true;
        G.pending_dir=dir;
        G.pending_state=state;
        G.pending_throttle=throttle;
        G.direction_stop_ms=now_ms();
        G.restart_deadtime_started=false;
        if(ms()!=MDRV_IDLE&&!stopping) motor_drv_stop(true);
        to(GS_IDLE_MID,stopping?"restart waits for stop":"direction change stop");
        return;
    }
    (void)set_motor_command(dir,throttle,state,reason);
}

static void process_pending_move(const TickPeriod_t *tp){
    if(motor_drv_state()==MDRV_FAULT){
        latch_fault("motor driver fault during direction change",ESP_OK);
        return;
    }
    if(ms()!=MDRV_IDLE){
        if((uint32_t)(tp->now_ms-G.direction_stop_ms)>=DIRECTION_STOP_TIMEOUT_MS)
            latch_fault("direction change stop timeout",ESP_OK);
        return;
    }
    if(!G.restart_deadtime_started){
        G.restart_deadline_ms=tp->now_ms+DIRECTION_CHANGE_DEAD_TIME_MS;
        G.restart_deadtime_started=true;
        return;
    }
    if((int32_t)(tp->now_ms-G.restart_deadline_ms)<0) return;

    gate_dir_t dir=G.pending_dir;
    uint8_t throttle=G.pending_throttle;
    gate_state_t state=G.pending_state;
    G.restart_pending=false;
    G.restart_deadtime_started=false;
    (void)set_motor_command(dir,throttle,state,"direction change restart");
}

esp_err_t gate_logic_init(void){
    memset(&G,0,sizeof G);
    G.homing_start_ms=now_ms();
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
    request_move(DIR_OPEN,storage_cfg()->travel_counts,
                 storage_cfg()->duty_max_pct,GS_MOVING_OPEN,"open command");
}
void gate_cmd_close(void){
    if(G.st==GS_FAULT) return;
    request_move(DIR_CLOSE,0,storage_cfg()->duty_max_pct,
                 GS_MOVING_CLOSE,"close command");
}
void gate_cmd_stop(void){
    bool homing=G.st==GS_CALIBRATING;
    bool moving=ms()!=MDRV_IDLE || G.restart_pending || homing ||
                G.st==GS_MOVING_OPEN || G.st==GS_MOVING_CLOSE ||
                G.st==GS_PINCH_REVERSE;
    G.restart_pending=false;
    G.restart_deadtime_started=false;
    G.homing_command_started=false;
    if(ms()!=MDRV_IDLE) motor_drv_stop(true);
    if(!moving) return;
    to(GS_IDLE_MID,"user stop");
    if(!homing) storage_save_position(encoder_position(),0,true);
}

void gate_vision_event(int kind,int vid){
    /* kinds: 1=APPROACH_OK (open), 2=INSIDE (hold), 3=LEFT_CLEAR (auto-close) */
    (void)vid;
    if(kind==1 && (G.st==GS_IDLE_CLOSED||G.st==GS_IDLE_MID)) gate_cmd_open();
    else if(kind==3 && G.st==GS_IDLE_OPEN) gate_cmd_close();
    /* kind==2 extends auto-close timer via power-less timer var (Phase D) */
}

static bool check_pinch_or_stall(const TickPeriod_t*tp){
    bool moving=(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE);
    if(!moving) return false;
    gate_dir_t cmd=G.st==GS_MOVING_OPEN?DIR_OPEN:DIR_CLOSE;
    if(encoder_status()==ENC_STALE){
        latch_fault("encoder stale",ESP_OK);
        return true;
    }
    if(!encoder_dir_matches(cmd,800)){
        latch_fault("encoder direction contradiction",ESP_OK);
        return true;
    }
    if(!storage_cfg()->pinch_detect_en) return false;
    if(moving && current_over_run(PINCH_CURRENT_HOLD_MS)){
        motor_drv_stop(false);
        to(GS_PINCH_REVERSE,"overcurrent");
        G.pinch_since_ms=tp->now_ms;
        events_log_write(EV_FAULT,"pinch I=%.2fA pos=%d",current_rms(),encoder_position());
        return true;
    }
    /* no-current watchdog: running yet ~0 A => belt snapped / triac dead */
    if(ms()==MDRV_RUNNING&&current_rms()<0.05f&&
       (uint32_t)(tp->now_ms-G.move_start_ms)>3000){
        latch_fault("no current",ESP_OK);
        return true;
    }
    return false;
}

void gate_logic_step(const TickPeriod_t *tp){
    btn_event_t b; while((b=buttons_pop())!=BTN_EV_NONE) handle_btn(b);

    const limit_state_t*L=limit_sw_state();
    int32_t pos=encoder_position();
    const gate_config_t*c=storage_cfg();

    if(G.restart_pending){
        process_pending_move(tp);
        motor_drv_update(tp);
        return;
    }

    /* ramp-down before limits using speed estimate */
    if(G.st==GS_MOVING_OPEN||G.st==GS_MOVING_CLOSE){
        int32_t rem = (G.st==GS_MOVING_OPEN)? c->travel_counts-pos : pos;
        uint32_t zone=c->travel_counts*RAMP_DOWN_ZONE_PCT/100;
        if(L->wire_fault||(L->at_closed&&L->at_open)){
            latch_fault("limit wire fault",ESP_OK);
        } else if(G.st==GS_MOVING_CLOSE && L->at_closed){
            motor_drv_stop(false); encoder_set_origin_closed();
            to(GS_IDLE_CLOSED,"closed"); storage_save_position(0,0,true);
        } else if(G.st==GS_MOVING_OPEN && L->at_open){
            motor_drv_stop(false); encoder_set_origin_open();
            to(GS_IDLE_OPEN,"opened"); storage_save_position(c->travel_counts,0,true);
        } else if(check_pinch_or_stall(tp)){
            /* The safety check changed the state and stopped the motor. */
        } else if((uint32_t)(tp->now_ms-G.move_start_ms)>MOVE_TIMEOUT_MS){
            latch_fault("move timeout",ESP_OK);
        } else if(rem>=0&&(uint32_t)rem<zone){
            esp_err_t err=motor_drv_set(G.st==GS_MOVING_OPEN?DIR_OPEN:DIR_CLOSE,45);
            if(err!=ESP_OK) latch_fault("ramp-down command",err);
        }
    } else if(G.st==GS_PINCH_REVERSE){
        if((uint32_t)(tp->now_ms-G.pinch_since_ms)>700){ /* pause then back off */
            gate_dir_t back=(G.target==0)?DIR_OPEN:DIR_CLOSE;
            request_move(back,back==DIR_OPEN?c->travel_counts:0,
                         c->duty_max_pct/2,
                         back==DIR_OPEN?GS_MOVING_OPEN:GS_MOVING_CLOSE,
                         "pinch reverse"); }
    } else if(G.st==GS_CALIBRATING){
        if(L->wire_fault||(L->at_closed&&L->at_open)){
            latch_fault("limit wire fault during homing",ESP_OK);
        } else if(L->at_closed){
            motor_drv_stop(false); encoder_set_origin_closed();
            to(GS_IDLE_CLOSED,"homed"); storage_save_position(0,0,true); }
        else if((uint32_t)(tp->now_ms-G.homing_start_ms)>=HOMING_TIMEOUT_MS){
            latch_fault("homing timeout",ESP_OK);
        } else if(!G.homing_command_started){
            if(set_motor_command(DIR_CLOSE,35,GS_CALIBRATING,"homing"))
                G.homing_command_started=true;
        }
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
