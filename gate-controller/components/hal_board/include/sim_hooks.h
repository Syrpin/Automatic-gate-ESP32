/*
 * sim_hooks.h — the ONLY surface production drivers may call when built with
 * CONFIG_GATE_SIM. Without that define the hooks compile to nothing and the
 * drivers read real GPIO/ADC (shipping builds carry zero test code).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_GATE_SIM
/* Extern declarations: implemented once in sim.c (built with
 * SIM_IMPLEMENTATION). Drivers that include this header must link against
 * the sim component when CONFIG_GATE_SIM is enabled. */
bool  sim_button_active(void);          /* true => buttons_sample reads sim levels   */
int   sim_button_level(int idx);        /* 0=OPEN 1=STOP 2=CLOSE; active-low level    */
bool  sim_limit_active(void);           /* true => limit_sw_scan reads sim levels     */
int   sim_limit_level(int id);          /* 0=CLOSED sw 1=OPEN sw; NC active-low level */
bool  sim_current_value(float *amps_out);/* injected or plant-model amps feed CT RMS  */
bool  sim_mains_state(bool *ok_out);    /* overrides mains detection in power_mgmt    */

/* Virtual mains zero-cross generator (regional 50/60 Hz testing). */
void  sim_zc_start(uint16_t hz);        /* 50 or 60; feeds motor_drv ZC ISR hook */
void  sim_zc_stop(void);
bool  sim_zc_running(void);
#else
static inline bool sim_button_active(void) { return false; }
static inline int  sim_button_level(int i) { (void)i; return 1; }
static inline bool sim_limit_active(void)  { return false; }
static inline int  sim_limit_level(int id) { (void)id; return 1; }
static inline bool sim_current_value(float *a) { (void)a; return false; }
static inline bool sim_mains_state(bool *ok)   { (void)ok; return false; }
static inline void sim_zc_start(uint16_t hz)   { (void)hz; }
static inline void sim_zc_stop(void)           {}
static inline bool sim_zc_running(void)        { return false; }
#endif

#ifdef __cplusplus
}
#endif
