#include <string.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "hal_board.h"
#include "motor_drv.h"
#include "limit_sw.h"
#ifdef CONFIG_GATE_SIM
#include "sim_hooks.h"
#endif

static const char *TAG = "lsw";
#define DEBOUNCE_TICKS 3   /* 30 ms */

static struct {
    uint8_t stable_closed, stable_open;   /* debounced levels        */
    uint8_t cnt_c, cnt_o;                 /* candidate counters      */
    limit_state_t st;
    sw_id_t last_evt; bool last_pressed;
    uint32_t fault_since_ms;
} L;

static inline int raw(sw_id_t id){
#ifdef CONFIG_GATE_SIM
    if (sim_limit_active()) return sim_limit_level(id==SW_CLOSED?0:1)==0; /* NC active-low */
#endif
    return gpio_get_level(id==SW_CLOSED?PIN_LIMIT_CLOSED:PIN_LIMIT_OPEN)==0; /* NC active-low */
}

esp_err_t limit_sw_init(void){ memset(&L,0,sizeof L); L.stable_closed=L.stable_open=1; return ESP_OK; }

void limit_sw_scan(const TickPeriod_t *tp)
{
    /* simple count-up debounce FSM per input */
    uint8_t rc = raw(SW_CLOSED), ro = raw(SW_OPEN);
    if (rc != L.stable_closed) { if (++L.cnt_c >= DEBOUNCE_TICKS){ L.stable_closed=rc; L.cnt_c=0;
        L.last_evt=SW_CLOSED; L.last_pressed=(rc==1); L.st.changed_ms=tp->now_ms; } } else L.cnt_c=0;
    if (ro != L.stable_open)  { if (++L.cnt_o >= DEBOUNCE_TICKS){ L.stable_open=ro; L.cnt_o=0;
        L.last_evt=SW_OPEN;  L.last_pressed=(ro==1); L.st.changed_ms=tp->now_ms; } } else L.cnt_o=0;

    L.st.at_closed = L.stable_closed;
    L.st.at_open   = L.stable_open;
    /* wire-break heuristic: moving away from a limit yet that limit still reads
       "engaged" for > travel timeout => contact/wire fault */
    bool moving = motor_drv_state()!=MDRV_IDLE;
    /* both engaged simultaneously is physically impossible; additionally, a
       limit still "engaged" while we are moving AWAY from it for >2s means a
       broken contact/wire (the old firmware never caught this at all) */
    bool both = L.st.at_closed && L.st.at_open;
    bool stale_engaged = moving && ((L.st.at_closed && motor_drv_direction()==DIR_CLOSE) ||
                                    (L.st.at_open   && motor_drv_direction()==DIR_OPEN));
    /* both-at-once is impossible; engaged-while-driving-into-it for >2s means
       the contact never broke => wire/contact fault (drillable via `fault both-limits`) */
    if (both || stale_engaged) {
        if (!L.fault_since_ms) L.fault_since_ms = tp->now_ms;
        L.st.wire_fault = (uint32_t)(tp->now_ms - L.fault_since_ms) > 2000;
    } else { L.fault_since_ms = 0; L.st.wire_fault = false; }
}

const limit_state_t *limit_sw_state(void){ return &L.st; }
sw_id_t limit_sw_last_event(bool *pressed){ *pressed=L.last_pressed; return L.last_evt; }
