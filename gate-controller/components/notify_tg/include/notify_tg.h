/*
 * notify_tg.h — Telegram Bot API over HTTPS (plan p.9.1 step4):
 * token/chat_id in NVS; 30s debounce dedupe against mains flicker; retry x2;
 * late-delivery of pending notifications after network recovery.
 */
#pragma once
#include "esp_err.h"
esp_err_t notify_tg_init(void);
esp_err_t notify_tg_send(const char *text);   /* queues, never blocks caller */
