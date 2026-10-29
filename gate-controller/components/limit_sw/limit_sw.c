#include <string.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "hal_board.h"
#include "motor_drv.h"
#include "limit_sw.h"

static const char *TAG = "lsw";
#define DEBOUNCE_TICKS 3   /* 30 ms */
/* A limit engaged while travelling away from it must release within this
   window; beyond it the reading is trusted as stuck-contact/wire-break. */
#define LIMIT_HOLD_MS 5000

static struct {
    uint8_t stable_closed, stable_open;   /* debounced "engaged" flags (NC logic) */
    uint8_t cnt_c, cnt_o;                 /* candidate counters      */
    int64_t engaged_since_ms;             /* when current limit engagement began  */
    bool moving_at_engage;                /* was motor running when it engaged?   */
    limit_state_t st;
    sw_id_t last_evt; bool last_pressed;
} L;

static inline int raw(sw_id_t id){
    return gpio_get_level(id==SW_CLOSED?PIN_LIMIT_CLOSED:PIN_LIMIT_OPEN)==0; /* NC active-low */
}

esp_err_t limit_sw_init(void){ memset(&L,0,sizeof L);
    /* NC contacts: idle = NOT engaged. Both "engaged" at boot is physically
       impossible -> flag wire-break immediately (broken common return). */
    L.stable_closed=L.stable_open=0; L.st.at_closed=L.st.at_open=false;
    L.st.wire_fault=false; L.engaged_since_ms=-1; return ESP_OK; }

void limit_sw_scan(const TickPeriod_t *tp)
{
    /* simple count-up debounce FSM per input */
    uint8_t rc = raw(SW_CLOSED), ro = raw(SW_OPEN);
    if (rc != L.stable_closed) { if (++L.cnt_c >= DEBOUNCE_TICKS){ L.stable_closed=rc; L.cnt_c=0;
        L.last_evt=SW_CLOSED; L.last_pressed=(rc==1); L.st.changed_ms=tp->now_ms;
        if(rc) L.engaged_since_ms=tp->now_ms; } } else L.cnt_c=0;
    if (ro != L.stable_open)  { if (++L.cnt_o >= DEBOUNCE_TICKS){ L.stable_open=ro; L.cnt_o=0;
        L.last_evt=SW_OPEN;  L.last_pressed=(ro==1); L.st.changed_ms=tp->now_ms;
        if(ro) L.engaged_since_ms=tp->now_ms; } } else L.cnt_o=0;

    L.st.at_closed = L.stable_closed;
    L.st.at_open   = L.stable_open;
    /* Wire-break detection for NC end-switches with a shared return wire:
       while the gate travels AWAY from an engaged limit, that limit must
       release within a short distance/time. If it stays clamped "engaged"
       for LIMIT_HOLD_MS while moving in the opposite direction, either the
       contact is welded or the return wire of the OTHER switch is broken
       (reads stuck). Both switches engaged simultaneously is also flagged. */
    bool both = L.st.at_closed && L.st.at_open;
    bool moving = motor_drv_state()!=MDRV_IDLE;
    bool stale = moving && L.engaged_since_ms>=0 &&
                 (int64_t)tp->now_ms - L.engaged_since_ms > LIMIT_HOLD_MS;
    L.st.wire_fault = both || stale;
}

const limit_state_t *limit_sw_state(void){ return &L.st; }
sw_id_t limit_sw_last_event(bool *pressed){ *pressed=L.last_pressed; return L.last_evt; }
