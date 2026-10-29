#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "events_log.h"

static const char *TAG="elog";
#define RING_N 64
#define LINE_MAX 160

static struct { char line[LINE_MAX]; } ring[RING_N];
static volatile uint8_t head,tail; static uint32_t dropped;

esp_err_t events_log_init(void){ return ESP_OK; }

void events_log_write(ev_kind_t k,const char*fmt,...){
    uint8_t nh=(head+1)%RING_N;
    if(nh==tail){dropped++;return;}
    va_list ap; va_start(ap,fmt);
    int64_t us=esp_timer_get_time();
    snprintf(ring[head].line,LINE_MAX,"{\"t\":%lld,\"k\":%d,\"m\":",(long long)(us/1000),(int)k);
    size_t used=strlen(ring[head].line);
    vsnprintf(ring[head].line+used,LINE_MAX-used,fmt,ap);
    strncat(ring[head].line,"}\n",LINE_MAX-strlen(ring[head].line)-1);
    va_end(ap);
    head=nh;
}

void events_log_flush(void){
    if(head==tail) return;
    nvs_handle_t h; if(nvs_open("log",NVS_READWRITE,&h)!=ESP_OK) return;
    while(head!=tail){
        /* store as multi-page string blob key seq; Phase E replaces with raw
           partition writer for wear leveling efficiency */
        static uint32_t seq; char key[8]; snprintf(key,sizeof key,"e%u",seq++&0xFFFF);
        nvs_set_str(h,key,ring[tail].line);
        tail=(tail+1)%RING_N;
    }
    nvs_commit(h); nvs_close(h);
}
uint32_t events_log_dropped(void){return dropped;}
