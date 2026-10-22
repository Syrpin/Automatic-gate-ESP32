#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "hal_board.h"
#include "buttons.h"

static const char *TAG="btn";
#define LONGPRESS_MS 800
#define DBNC_MS      50

typedef struct { int pin; uint32_t down_ms; bool stable,last; uint8_t hist; } bstate_t;
static bstate_t B[3]={ {PIN_BTN_OPEN},{PIN_BTN_STOP},{PIN_BTN_CLOSE} };
static btn_event_t Q[4]; static uint8_t qh,qt;

static void push(btn_event_t e){ if(e&&((qt+1)&3)!=qh){Q[qt]=e;qt=(qt+1)&3;} }

esp_err_t buttons_init(void){ return ESP_OK; }

static btn_event_t classify(uint8_t idx,bool pressed,uint32_t held_ms){
    if(!pressed){ /* release: emit short if it was a quick tap */
        return (held_ms>DBNC_MS&&held_ms<LONGPRESS_MS)?
            (idx==0?BTN_OPEN_SHORT: idx==1?BTN_STOP_SHORT: BTN_CLOSE_SHORT): BTN_EV_NONE; }
    if(held_ms>=LONGPRESS_MS) return idx==0?BTN_OPEN_LONG: idx==2?BTN_CLOSE_LONG: BTN_EV_NONE;
    return BTN_EV_NONE;
}

void buttons_sample(const TickPeriod_t *tp){
    for(uint8_t i=0;i<3;i++){
        int lvl=gpio_get_level(B[i].pin); bool raw=!lvl; /* active-low, pull-up */
        B[i].hist=(B[i].hist<<1)|(raw&1);
        bool stbl = (B[i].hist&7)==7 ? true : (B[i].hist&7)==0 ? false : B[i].stable;
        if(stbl!=B[i].last){
            if(stbl) B[i].down_ms=tp->now_ms;
            else { uint32_t held=tp->now_ms-B[i].down_ms;
                   if(held<LONGPRESS_MS) push(classify(i,true,held)); }
            B[i].last=stbl;
        } else if(stbl){ push(classify(i,true,tp->now_ms-B[i].down_ms)); }
        B[i].stable=stbl;
    }
}
btn_event_t buttons_pop(void){ if(qh==qt) return BTN_EV_NONE; btn_event_t e=Q[qh]; qh=(qh+1)&3; return e; }
bool buttons_any_pressed(void){ return B[0].stable||B[1].stable||B[2].stable; }
