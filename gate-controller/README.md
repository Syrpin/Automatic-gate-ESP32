# gate-controller (ESP32-S3 port, Этап A–B skeleton)

Перенос прошивки ворот STM32F103 -> ESP-IDF (ESP32-S3 WROOM-1-N16R8), см.
/ESP32_PORT_PLAN.md (разделы 9–12 — принятые решения).

## Сборка
```
idf.py set-target esp32s3
idf.py build flash monitor
```
Требуется ESP-IDF >= v5.2. Makefile сознательно не используется (решение пользователя).

## Структура (компоненты = этапы плана)
| Компонент | Назначение | Готовность скелета |
|---|---|---|
| hal_board | пин-карта devkit, safe-state, профиль платы | готово |
| storage_nvs | конфиг+валидация, позиция+trusted, pending notify | готово |
| motor_drv | симистор phase-angle + реле K1/K2/K3, dead-time, ramps | логика готова; ISR-подключение ZC и one-shot firing — Phase B bring-up |
| encoder | оптический/DX/DI, GPIO-ISR на devkit, PCNT на кастоме, overflow-safe | декодер готов; PCNT init — Phase B |
| limit_sw | debounce + wire-fault | готово |
| buttons | FSM short/long press, очередь событий | готово |
| current_sense | RMS окно, stall/inrush/no-current | oneshot-скелет -> DMA в Phase B |
| gate_logic | state machine (замена CONDITION-маски), анти-защемление, triple validation | готово |
| power_mgmt | mains-loss budget p.9.1, батарея, thermal interlock hook | каркас; fast-path детект — Phase B |
| events_log | RAM ring + JSONL journal | готово (NVS-ключи; raw-writer Phase E) |
| camera_pipeline / vision_core | API сценариев A/B (p.12) | заглушки Phase C/D |
| notify_tg | Telegram HTTPS, дедуп, late-delivery | заглушка Phase D |

main.c: ctrl_task@10ms, beep@50ms, telemetry@1s — без блокирующих циклов.

## Известные TODO скелета (не молча, а явно)
1. `gpio_install_isr_service()` вызывается в encoder_init; обработчик ZC для
   motor_drv_zero_cross_us() подключить там же (одна установка сервиса).
2. CT_LSB_TO_AMPS=0 — калибровочный усилитель тока из wizard (Phase E).
3. beep LEDC tone — фаза B.
4. known_pinch reverse после отката требует повторного cmd (сейчас продолжает
   движение к target) — уточнить политику (double-back-off vs stop).
5. partitions.csv: model_a/model_b 4MB каждый — под int8 tflite (esp-dl).

## Конфликт пинов devkit (честно)
DVP-пины пересекаются с sensing-пинами (см. комментарии hal_board.h):
на макете камера подключается частично (или sensor-тест отдельно); финальная
совместимость достигается на кастомной плате v1 — это и был план p.11.1.

## Regional profile & CT calibration (current build)
- `region eu|us|auto` console command and the Web UI Region selector commit
  mains_freq + region_code together through the validated NVS path (EU+60/US+50
  contradictions are rejected). The python twin (`tools/webui/serve.py`) mirrors
  the same API and persists to config.json.
- CT front end is now formula-driven: `current_sense_set_front_end(K_mV_per_A,
  VCC_mV, I_full_A, gain)` computes G=(3100−VCC/2)/(K·I_full); midrail is
  measured at boot (motor off), RMS window spans integer mains half-cycles.
  Verified on host: full-scale error ≤0.1 % for ACS712-30A/20A/05B profiles.
- See SCHEMATIC.md for the legacy-board review (D1..D8 defects) and the v1
  custom-board spec (MOC3052 random-phase optos, RC snubbers, RRIO amp,
  DC-DC+low-noise LDO power tree, pin map contract).
