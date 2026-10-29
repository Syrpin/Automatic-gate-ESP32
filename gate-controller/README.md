# gate-controller (ESP32-S3 port, Этап A–B)

Перенос прошивки ворот STM32F103 -> ESP-IDF (ESP32-S3 WROOM-1-N16R8), см.
/ESP32_PORT_PLAN.md (разделы 9–12 — принятые решения).

## Сборка
```
idf.py set-target esp32s3
idf.py build flash monitor
```
Требуется ESP-IDF >= v5.2. Makefile сознательно не используется (решение пользователя).

Host-side проверка синтаксиса/типов без железа и без ESP-IDF:
```
tools/host_check/check.sh        # gcc -fsyntax-only по заглушкам tools/host_check/fake/
```
Все 15 .c файлов компонентов+main проходят проверку (zero warnings при -Wall -Wextra).

## Документация

- [docs/HARDWARE_DESIGN.md](docs/HARDWARE_DESIGN.md) — проектирование принципиальной платы v1: требования, изоляция, pin-map (синхронно с `hal_board.h`), силовая часть (phase-angle + реле), сенсорный тракт (энкодер/концевые/CT/батарея/NTC), UPS, EMC, BOM, bring-up checklist, открытые вопросы до layout.

## Структура (компоненты = этапы плана)
| Компонент | Назначение | Готовность |
|---|---|---|
| hal_board | пин-карта devkit/custom, safe-state, общий GPIO ISR-сервис | готово |
| storage_nvs | конфиг+валидация с откатом к дефолтам, позиция+trusted, pending notify | готово |
| motor_drv | симистор phase-angle + реле K1/K2/K3, dead-time, ramps; ZC ISR подключён | логика+ISR готовы; one-shot firing таймер — Phase B bring-up на железе |
| encoder | оптический/DX/DI, GPIO-ISR на devkit, PCNT на кастоме, overflow-safe | декодер готов; PCNT init — Phase B |
| limit_sw | NC-debounce + wire-fault detection | готово |
| buttons | FSM short/long press, очередь событий | готово |
| current_sense | RMS окно, stall/inrush/no-current; fail-safe при некалиброванном CT | oneshot-скелет -> DMA в Phase B |
| beep | LEDC тон-движок, неблокирующие паттерны | готово |
| gate_logic | state machine (замена CONDITION-маски), анти-защемление, retry реверса, triple validation | готово |
| power_mgmt | mains-loss watchdog по ZC-gap, батарея, NTC-ингибирование заряда | готово; fast-path esp_timer — Phase B |
| events_log | RAM ring + JSONL journal | готово (NVS-ключи; raw-writer Phase E) |
| camera_pipeline / vision_core | API сценариев A/B (p.12) | заглушки Phase C/D |
| notify_tg | Telegram HTTPS, дедуп, late-delivery | каркас; сетевая часть Phase D |

main.c: ctrl_task@10ms (task WDT 200ms, panic), beep@50ms, telemetry@1s —
без блокирующих циклов; общий GPIO ISR-сервис устанавливается один раз до
инициализации драйверов (encoder A/B + mains ZC на одном сервисе).

## Известные TODO (не молча, а явно)
1. One-shot esp_timer для угла отпирания симистора (задержка от ZC) — только
   на железе (bench-тест без сети невозможен); hook `motor_drv_zero_cross_us()`
   уже вызывается из реального ZC ISR.
2. CT_LSB_TO_AMPS=0 — калибровочный усилитель тока из wizard (Phase E);
   защиты по току до калибровки осознанно выключены (fail-safe предупреждение
   в логе init).
3. K3-shunt (байпас симистора на полном ходу) отложен: коммутация под нагрузкой
   сваривает контакты — нужен zero-current window + счётчик циклов реле в NVS.
4. known_pinch reverse после отката требует повторного cmd (сейчас продолжает
   движение к target) — уточнить политику (double-back-off vs stop).
5. partitions.csv: model_a/model_b 4MB каждый — под int8 tflite (esp-dl).

## Конфликт пинов devkit (честно)
DVP-пины пересекаются с sensing-пинами (см. комментарии hal_board.h):
камера включается только в билде с GATE_CAM_ENABLED=1 (профиль custom-v1);
на devkit-макете работает вариант "sensing only". Финальная совместимость —
кастомная плата v1 (план p.11.1).
