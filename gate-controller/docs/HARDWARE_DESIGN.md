# Принципиальная плата Gate Controller v1 (ESP32-S3) — документация по проектированию

**Статус:** проект предварительный (rev A). Назначение документа — исходные данные для
схемы и платы в KiCad/Altium. Код прошивки (`components/*`, pin-map в
`components/hal_board/include/hal_board.h`) является источником истины по номерам GPIO;
любое расхождение схемы с `hal_board.h` считается ошибкой схемы.

Связанные разделы плана: p.11.1 (pin map), p.13 (политика IR/ночи), Phase B (аналог),
Phase E (калибровочный wizard).

---

## 1. Общие требования

| Параметр | Значение | Комментарий |
|---|---|---|
| Питание силовое | 230 VAC ±15 %, 50 Гц | фазово-угловое (phase-angle) управление однофазным асинхронным двигателем приворота |
| Двигатель | до 700 Вт номинал, пусковой ток до 6×Iном (~18 A пик) | выбрать реле/симистор с запасом ×3 по току |
| Логическое питание | 3.3 В, ≤ 600 мА | Wi-Fi bursts ~350 мA пик |
| Дежурное питание / UPS | буфер 12 В АКБ 1.2–7 А·ч через DC/DC | удержание логики + аварийное закрытие створки при пропадании сети (бюджет <10 мс на обнаружение mains-lost) |
| MCU | ESP32-S3-WROOM-1-N16R8 (16 МБ flash, 8 МБ octal PSRAM) | PSRAM нужна для later-этапа tflite; octal PSRAM занимает GPIO33–37 — они запрещены к использованию |
| Рабочая температура | −30 … +60 °C (улица, козырёк) | NTC-интерлок батареи ограничивает заряд при T<0 °C и T>45 °C |
| Защита | IP66 корпус, отдельная силовая камера | все сетевые цепи — только в силовой камере |
| Безопасность | гальваническая изоляция сети от логики ≥ 3 кВ RMS | усиленная изоляция (reinforced), creepage ≥ 6.4 мм между первичной/вторичной сторонами |

### 1.1 Нормативка
- EN 60335-1 / EN 60335-2-103 (приворота): обязательное аппаратное отключение, ограничение усилия (anti-pinch ≤ заданного порога), концевые выключатели как независимый канал безопасности.
- IEC 61000-4-x: EFT 2 кВ на сетевой порт, surge 1 кВ L-L / 2 кВ L-PE, CS 30 А/м на силовые входы.
- UL/CE подготовка: Y2-конденсаторы между первичной и вторичной землями, X2 на входе, varistor MOV 275 VAC.

---

## 2. Архитектура платы (блок-схема)

```
230VAC ──[FUSE 5A T]──[MOV+X2]──┬────── AC-DC (HLK-PM12, isolated) ──[12V rail]──[ideal diode OR-ing with 12V battery]──[Buck 3.3V TPS563201]
                                 │                                                        │
                                 ├──[divider → H11AA1 opto]── GPIO14 (ZC + mains-loss)   LOGIC GND (isolated secondary)
                                 │
                                 ├──[K1/K2 direction relays]──┐
                                 ├──[K3 shunt relay]          ├─ TRIAC BTA16 + snubber ── MOTOR (L/N + PE)
                                 │                            │
                            [MOC3041 opto-triac driver]── GPIO10 PWM (LEDC/MCPWM0A)
                                 │
LOGIC SIDE (3.3V): ESP32-S3 — encoder GPIO15/16, limits GPIO17/18 (NC), buttons GPIO4/5/6,
beep GPIO7, LED GPIO38, CT GPIO1 (ADC1_CH0), battery GPIO2 (ADC1_CH1), NTC GPIO8 (ADC1_CH6),
RTC/I2C GPIO9/40, UART0 43/44, USB-C on 19/20, camera DVP (см. §7, отдельный build variant).
```

Два гальванических острова: **MAINSIDE** (силовая камера) и **LOGIC** (логическая плата).
Связь через оптроны (управление симистора, ZC-детекция). Общая точка — только защитное
заземление PE. Вторичка HLK-PM12 и логическая земля соединены через точку STAR-GND buck.

---

## 3. Pin-map финальный (custom board v1, CONFIG_GATE_BOARD_CUSTOM_V1=1)

Нумерация ниже = номера GPIO ESP32-S3, синхронизировано с `hal_board.h`. Правило CI
(`tools/pinmap_check.py`): каждый GPIO не более одной роли.

### 3.1 Цифровые, сеть/актуаторы
| GPIO | Сеть | Роль | Тип | Примечание |
|---|---|---|---|---|
| 10 | MAINS | PIN_TRIAC_PWM | out, LEDC/MCPWM0A | 20 кГц, R 1 кОм → LED оптрона; pull-down 100 кОм на triac gate (fail-off) |
| 11 | LOGIC | PIN_RELAY_OPEN (K1) | out, Q1 2N7002 | coil 12 В, диод SS54антипараллельно |
| 12 | LOGIC | PIN_RELAY_CLOSE (K2) | out, Q2 | dead-time 60 мс в софте; аппаратно K1/K2 взаимоблокировка NC-допами контактов |
| 13 | LOGIC | PIN_RELAY_SHUNT (K3) | out, Q3 | шунт triac на полном ходу; включать только в zero-current окне (fw policy) |
| 14 | MAINS→LOGIC | PIN_MAINS_DETECT | in, NEGEDGE ISR | open-collector выход H11AA1, pull-up 10 кОм к 3.3 В |

### 3.2 Датчики
| GPIO | Роль | Тип | Примечание |
|---|---|---|---|
| 15 | ENC_A | in, GPIO-ISR (PCNT0 sig0 на custom) | RC 1 кОм/10 нФ + Schmitt 74LVC1G14; pull-up 10 кОм (датчик OC) |
| 16 | ENC_B | in, PCNT0 sig1 | тоже |
| 17 | LIMIT_CLOSED | in | NC-контакт, active-low; фильтр 1 кОм/100 нФ; debounce 10 мс в limit_sw.c |
| 18 | LIMIT_OPEN | in | аналогично; ОБА разомкнуты одновременно = обрыв провода (wire-break detect) |
| 21 | PROBE_OBSTACLE | in, опционально | механическая полоса; конфликтует с CAM_VSYNC в cam-build — per-build jumper |

### 3.3 UI
| GPIO | Роль | Примечание |
|---|---|---|
| 4 | BTN_OPEN | RTC-capable, ext1 wake; internal pull-up включён в buttons.c |
| 5 | BTN_STOP | RTC-capable |
| 6 | BTN_CLOSE | RTC-capable |
| 7 | BEEP | passive buzzer 3.3 В через NMOS SS8050, LEDC ch; Шоттки на катушку зуммера |
| 38 | STATUS_LED | plain LED + 1 кОм; в cam-build разделяется с IR_ILLUM (перемычка JP_LED/JP_IR) |

### 3.4 Аналог (ADC1 только! ADC2 конфликтует с Wi-Fi)
| GPIO | Канал | Сигнал | Фронтенд |
|---|---|---|---|
| 1 | ADC1_CH0 | CT clamp RMS | SCT-024-009 + burden → AD8237 iso-instr amp → bias 1.65 В (REF3033) → LPF 1 кГц → TVS clamp |
| 2 | ADC1_CH1 | Battery voltage | делитель (см. §5.4 — ревизия ratio!), LPF 100 нФ |
| 8 | ADC1_CH6 | NTC battery temp | 10 кОм NTC B=3950 + 10 кОм upper, LPF 100 нФ |

### 3.5 Прочее
| GPIO | Роль | Примечание |
|---|---|---|
| 9, 40 | I2C SCL/SDA | pull-up 4.7 кОм; DS3231 RTC + 24LC256 EEPROM |
| 43, 44 | UART0 TX/RX | debug header; boot logs — не занимать |
| 0 | BOOT | только кнопка bootloader |
| 45, 46 | strapping VDD_SPI / RF_SW | не использовать как outputs |
| 19, 20 | USB-Serial-JTAG D+/D− | USB-C provisioning/console; ESD USBLC6-2SC6 |
| 26–32 | SPI flash | NEVER ROUTE |
| 33–37 | octal PSRAM | NEVER ROUTE (N16R8) |
| 47, 48 | свободны в control-build | CAM_D1/D0 в cam-build |

---

## 4. Силовая часть (MAINSIDE)

### 4.1 Входная защита и EMC
- **F1**: предохранитель 5 A / 250 V time-launch (T5A), разборный держатель.
- **RV1**: MOV 14D471K (275 VAC, Imax 6 кА) L-N.
- **CX1**: X2 0.47 мкФ / 310 VAC.
- **LF1**: синфазный дроссель 10 мГн + CY1/CY2 Y2 2.2 нФ (после дросселя к PE).
- Всё после F1.

### 4.2 Фазирующий регулятор (phase-angle)
- **TR1**: симистор **BTA16-600BW** (16 A, 600 В, dV/dt ≥ 50 V/µs), TO-220 на изолированной колодке, радиатор RθA ≤ 4 К/Вт (~1.8 Вт при 3 A RMS; запас по пусковому току обязателен).
- **Snubber**: 33 Ом / 0.22 мкФ X2-class параллельно MT1–MT2 — гасит dV/dt индуктивной нагрузки. Проверить нагрев резистора на пуске.
- **Драйвер**: MOC3041 (random-phase, БЕЗ zero-cross внутри) + DIAC DB3 + R gate 36 Ом. Zero-cross-opto (MOC3023) НЕ годится — нужен произвольный угол.
- **PWM-вход**: GPIO10 → R 1 кОм → LED оптрона; pull-down 10 кОм (MCU reset = triac off). Частота LEDC 20 кГц, duty = угол зажигания.

### 4.3 Реле направления K1/K2/K3
- **Реле**: Omron G5NB-1A-E 12VDC (16 A contacts); для K3 — версия с золотыми контактами (риск залипания критичен).
- **Mutual lock**: NC-допы K1 и K2 последовательно в катушки противоположного реле — аппаратный запрет одновременного включения.
- **Drive**: 2N7002, R gate 1 кОм, pull-down 100 кОм (GPIO floating during reset = relay released),freewheel SS54.
- **K3 shunt**: замыкается только при полностью открытом triac (full speed) — экономит ~1.5 Вт; fw включает его только в zero-current окне (motor_drv.c TODO hw bring-up).

### 4.4 Zero-cross / mains-loss detection (GPIO14)
Цепь: L → R divider 2×470 кОм/0.25 Вт → **H11AA1** (anti-parallel вход) → выход open-collector, pull-up 10 кОм к 3.3 В (LOGIC сторона).
- Два импульса на период (±порог) — fw считает half-period (MAINS_ZC_GAP_MS=35 мс tolerance, power_mgmt.c).
- Loss-of-mains: отсутствие фронта > 3×20 мс, debounce 15 мс (MAINS_LOST_DEBOUNCE_MS) → emergency path (бюджет <10 мс).
- Задержка propagation оптопара typ 1 мс — учесть при калибровке one-shot firing timer (TODO на железе).

### 4.5 AC/DC вспомогательное + UPS
- **PSU1**: Mean Well **HLK-PM12** (12 В / 0.83 А, isolated 4 кВ).
- Buck LOGIC: **TPS563201** (3.3 В / 3 А, 4.5–17 В). Вход 10 мкФ×2, выход 22 мкФ×2 + 100 нФ. Тестовые точки EN/3V3.
- UPS path: АКБ 12 В → ideal diode **LTC4412** (или SS56 Schottky — дешевле, падение 0.5 В) OR-ing с PSU1; разряд ограничен 10.5 В (fw low-batt policy BATT_LOW_PCT=10).

---

## 5. Сенсорный тракт

### 5.1 Энкодер (магнитный incremental, OC-выход)
- Pull-up 10 кОм к 3.3 В, RC 1 кОм+10 нФ прямо у модуля ESP32, затем Schmitt 74LVC1G14 (5V-tolerant inputs — можно датчик с 5 В).
- Питание датчика стабильное; длинная линия до ворот (≤15 м) витой парой, экран к LOGIC GND.
- На custom-плате включить PCNT0 (hw count) — fw переключается флагом `enc_use_gpio_isr=false`.

### 5.2 Концевые выключатели (независимый канал безопасности)
- NC-контакты: wire-break = оба разомкнуты = отказ (реализовано в limit_sw.c).
- Разъём J_LIMIT 4-pin JST-XH (SW1, SW2, 3V3 loop-back, GND), контакты IP67, кабель ≤15 м.
- Фильтр 1 кОм/100 нФ + software debounce 10 мс.
- Концевые — единственный разрешённый hard-stop; anti-pinch по току — дополнение, не замена.

### 5.3 Токовый трансформатор (CT)
- **CT1**: SCT-024-009 (24 A : 9 мА) split-core. Burden 33 Ом → ~0.3 В пик при 10 A RMS.
- **AD8237** gain ≈ 5 → дифф 1.5 В → single-ended вокруг bias 1.65 В (REF3033) → LPF fc≈1 кГц → GPIO1 через R 1 кОм + TVS clamp 0.3/3.3 В.
- Пусковой ток 18 A клиппует вход — fw трактует clip как inrush окно (`inrush_window_ms` в gate_logic.c), не как pinch.
- Калибровка gain хранится в NVS (`current_sense_set_gain()`, wizard Phase E); до калибровки fw сам блокирует токовые защиты (fail-safe реализован).

### 5.4 Измерение батареи / NTC
- BAT: делитель верхнее плечо 150 кОм / нижнее 100 кОм → ratio 2.5 (12.0 В → 3.0 В на GPIO2, 10.5 В → 2.6 В). **ВНИМАНИЕ:** в power_mgmt.c сейчас `BATT_DIVIDER 2.0f` — при принятии этого номинала синхронно поменять код, README и данный документ (иначе low-batt threshold врёт). Компромисс ratio 2.0 (100k/100k) оставляет 6.0 В на делителе при 12 В — выше 3.3 В ADC! Обязателен clamp или больший ratio.
- NTC: 10 кОм B3950 epoxy sealed на корпусе АКБ, series 10 кОм upper к 3.3 В, node → GPIO8, LPF 100 нФ. fw: charge inhibit T<0/+45 °C (`power_charge_inhibited()`).

---

## 6. Корпус, разъёмы, монтаж

| Позиция | Обозначение | Описание |
|---|---|---|
| Силовая камера | J_MOTOR 3-pin screw terminal 5.0 мм (L, N, PE) | 16 A rated, shroud |
| | J_ACIN 2-pin + PE, faston 6.35 мм с крышкой | fuse holder inside |
| | DIN-рейка 35 мм | PSU, клеммы |
| Логическая плата | J_ENC 4-pin JST-XH (A,B,+3V3,GND) | shielded twisted |
| | J_LIMIT 4-pin JST-XH | |
| | J_PROBE 3-pin JST-XH | optional |
| | J_BTN 6-pin (3 кнопки + common GND) | выносная панель |
| | USB-C 16-pin middle-mount | provisioning + console (USB-JTAG 19/20), CC resistors 5.1k |
| | UART0 testpads TX/RX/GND | production log |
| | J_BATT Micro-Fit 4.2 мм 2-pin | АКБ 12 В |
| | Антенна: u.FL + chip footprint | keep-out под модулем обязателен |
| Крепёж | 4× M3 standoffs; potting зон HV | conformal coating логики |

Компоновка: HV дорожки ≥ 3 мм (230 В, pollution degree 2); сетевой вход в углу;
opto isolation boundary — сплошной слот 6 мм в laminate между островами; антенна
≥ 5 см от симистора и дросселя; SI23xx MOSFET buck — thermal vias 4×0.3 мм.

---

## 7. Камера (OV5640 DVP) — build variant, НЕ в базовой плате v1

Модуль OV5640 через 20-pin FPC; пины из блока cam `hal_board.h`:
XCLK 35, SIOD 34, SIOC 36, D0..D7 = 48/47/44/43/42/41/40/39, VSYNC 21*, HREF 8*, PCLK 1*.
Звёздочки = конфликты с control-side (PROBE/NTC/CT). Решения для custom v1:
- **вариант A (рекомендуется)**: дочерняя плата камеры, общие сигналы через analog switch / IO-expander PCA9536 с default-disconnect при boot;
- **вариант B**: separate camera-only PCB spin (те же footprints MCU, другая трассировка).
IR-illum GPIO38 shared с STATUS_LED — перемычка JP_LED/JP_IR.
Питание камеры 5 В (на модуле свои LDO 2.8/1.5 В) — buck TPS62A02 с 12 В, LC-фильтр.

---

## 8. Стратегия тестирования платы (bring-up checklist)

1. **Bare-board power**: 220→12 В без нагрузки; ripple < 50 мВ; 3.3 В load step 0→350 мА — просадка < 100 мВ.
2. **Isolation**: hi-pot 3 кВ RMS primary↔secondary 60 с ≤ 1 мА; визуальный контроль slot/creepage.
3. **ZC chain**: scope GPIO14 vs mains — посчитать propagation delay, заложить в one-shot timer (fw TODO).
4. **Relay logic**: dead-time 60 мс measured; mutual-lock continuity test; K3 never closes while triac conducting (scope).
5. **Phase-angle**: RL dummy 300 Вт — sweep угла 30…170°, snubber R temp < 70 °C.
6. **CT calibration**: known load 2/5/10 A → fit gain (wizard Phase E); проверить clip на 6× пуске.
7. **Encoder/limits**: эмуляция кабеля 15 м, EFT 1 кВ на линию.
8. **UPS switchover**: mains cut at worst phase → det ≤ 10 мс, relay state preserved.
9. **Thermal soak**: 60 °C, 20 мин непрерывных циклов, NTC telemetry sane.
10. **EMC pre-scan**: CISPR 32 class B, near-field probes на сетевых проводах.

---

## 9. BOM (ключевые позиции, rev A)

| Ref | Part | Qty | Note |
|---|---|---|---|
| U1 | ESP32-S3-WROOM-1-N16R8 | 1 | |
| TR1 | BTA16-600BW | 1 | TO-220 insulated |
| OPTO1 | MOC3041 | 1 | random-phase gate drive |
| OPTO2 | H11AA1 | 1 | ZC / mains present |
| K1..K3 | Omron G5NB-1A-E 12VDC (K3 gold-clad) | 3 | |
| PSU1 | Mean Well HLK-PM12 | 1 | isolated 12 V |
| U2 | TPS563201 | 1 | 3.3 V buck |
| U3 | AD8237 | 1 | CT front-end |
| U4 | REF3033 | 1 | 1.65 V bias (÷2 from 3.3 В) |
| CT1 | SCT-024-009 | 1 | split-core |
| RTC1 | DS3231M | 1 | |
| U5 | 24LC256 | 1 | EEPROM резерв NVS |
| Q1..Q5 | 2N7002 / SS8050 | 5 | relay/buzzer drivers |
| D | SS54, 1N4148WS, DB3, SMBJ26A | — | |
| F1 | T5A/250V holder | 1 | |
| RV1 | 14D471K | 1 | |
| CX1 | X2 0.47 µF | 1 | |
| CY1-4 | Y2 2.2 nF | 4 | |
| LF1 | CM choke 10 mH | 1 | |
| NTC1 | 10k B3950 epoxy | 1 | на АКБ |
| BAT1 | 12V 1.2Ah SLA (или Li-ion 3S) | 1 | external |

---

## 10. Открытые вопросы / решения до layout

1. **Battery divider ratio** (§5.4): принять 2.5 и синхронно поменять `BATT_DIVIDER` в коде; иначе ADC перегружен при полном АКБ.
2. **CT gain & burden** (§5.3): финальный пересчёт + wizard-калибровка; TVS clamp обязателен.
3. **K3 zero-current window** — подтверждение осциллографом; иначе убрать K3 (экономия marginal).
4. **Camera interconnect** — выбрать вариант A/B (§7) до трассировки.
5. **Strapping GPIO45/46** — не выводить на внешние цепи.
6. **PSRAM/GPIO33-37 keep-out** — сверка footprints перед release.
7. **One-shot firing timer** — калибровка задержки ZC→угол на железе (fw TODO).

---
*Документ составлен по состоянию репозитория (pin-map = hal_board.h коммита 11487b6); при изменении прошивки обновлять §3, §5, §7.*
