# SCHEMATIC.md — оценка legacy-схемотехники привода ворот и целевая схема v1 для ESP32-S3

Документ: (1) разбор восстановленной схемы 2009 г. (AT89S52, MOC3023+BTA12, ACS712+LM358,
трансформатор+78xx), что в ней верно, что устарело и что опасно; (2) спецификация новой
платы v1 под наш проект (ESP32-S3-WROOM-1-N16R8, «симистор-реле», OV5640, батарея-UPS);
(3) карта соответствия `SCHEMATIC ↔ hal_board.h ↔ current_sense.c`.

---

## 1. Оценка legacy-схемы

### Что было сделано правильно (сохраняем идею)
- Гальваническая развязка от сети трансформатором — остаётся лучшим отказоустойчивым решением
  для ворот на улице (буревые наводки по линии 220 В).
- Опторазвязка управления симисторами (MOC30xx).
- RC-снабберы и ограничительные резисторы gate.
- Защитные диоды 1N4148 + pull-up на длинных кабелях концевиков/кнопок — правильная топология
  для «сухого контакта» на 20+ м.
- Раздельные земли power/logic со звездой под C1.

### Критические дефекты (в новой схеме не повторяем)
| # | Дефект legacy | Риск | Решение v1 |
|---|---|---|---|
| D1 | MOC3023 (zero-cross) + фазовое регулирование | ZC-оптопара физически не может зажечь симистор в середине полуволны — весь phase-angle мягкий пуск работать не будет | **MOC3052** (random-phase) или прямой drive H11G3+TRIAC; ZC-информация берётся отдельным компаратором |
| D2 | BAT137 назван «диодом»; снаббер R-C отсутствует как пара | dV/dt-пробой BTA12 при индуктивной нагрузке; самопроизвольный пуск мотора | **RC-снаббер 33Ω+47nF/630V** (или NTP5700N snubber) на каждый MT-MT симистора + TVS |
| D3 | ACS712 в «разрыв фазы мотора» без creepage, питание 5 В при ±100 мВ/A шуме | дрейф midrail = ложное анти-защемление; 20 A версия на пределе для 2100 Вт (9.5 A + пусковые) | ACS712-**30A**, питание строго 3.3 В (ratiometric!), экранированная трасса, см. §2 |
| D4 | LM358: crossover distortion у 2048 LSB, выход не rail-to-rail, усиление фиксировано делителями | «ступенька» в оцифровке около нуля сигнала — пропуск мягких касаний | **MCP6002/MCP6022 RRIO** (или OPA344), двухполярный бюджет G=(Vref−VCC/2)/(K·I_full), расчёт в current_sense.c |
| D5 | Линейные 7815→7805 каскадом: ~4.5 Вт тепла на 200 мА нагрузки, старт при −? не гарантирован | перегрев в гермичном корпусе летом, деградация | **MP1584/SY8089 DC-DC 3.3 В** + LDO 3.3 В аналоговый (AMS1117-3.3 или TPS7A02) для ACS712/ADC — чистая аналоговая шина |
| D6 | AT89S52: нет HW-энкодера, нет ADC, ISP 2x5 рядом с силовой зоной | — | ESP32-S3: PCNT×8, MCPWM, 12-bit ADC, USB-OTG debug |
| D7 | Нет никакого watchdog и supervision драйвера | зависание = ворота двигаются бесконечно | **драйвер с EN/FAULT-входом UCC21520/IR2110s с EN** + GPIO heartbeat + RTC WDT (в motor_drv уже заложен no-current watchdog) |
| D8 | Индикация/реле на MMBT4401/4403 без flyback-диодов на реле (в дампе не заявлены) | пробой ключей выбросами | 1N4148 антипараллельно каждой релейной обмотке + опто-развязка реле K1–K3 (см. §2) |

### Спорные пункты дампа, требующие уточнения на кастоме
- «TR1/TR2 коммутируют обмотки вперед/назад» — классическая реверсивная схема однофазного
  мотора требует переключения *фазосдвигающего конденсатора* либо перестановки фази/нуля;
  our topology (K1 фаза / K2 ноль / K3 shunt + симисторы) должна быть зафиксирована
  принципиальной схемой мотора ДО разводки платы.
- «Пружинная антенна 433 МГц» — на v1 заменяется BLE/Wi-Fi через модуль S3; если нужен
  штатный 433-пульт — отдельный CC1101 по SPI, не STM-наследие.

---

## 2. Целевая цепь измерения тока (реализована в current_sense.c)

```
Motor line ──┤ IP+ ACS712-30A IP├── 
                 VCC(3.3A)─┤ C1 0.1µF ├─ AGND
                 MFP(out) ──┬── Rf 10k ──┬── AGND     (anti-alias fc≈1.6 kHz c Cf... )
                           │             MCP6002 non-inv, G = 1 + R2/R1
        AGND ref ── VCC/2 divider (2×10k, buf) ── Rg 10k ──┘
        out ── 100Ω ──[10nF AGND]──► GPIO1 (ADC1_CH0)   (ESD: PESD5V0 ×2)
```
Бюджет усиления (прошит в коде формулой): usable span = 3.10 В (ATTEN_DB_12) − 1.65 В = 1.45 В;
ACS712-30A: K=66 мВ/А → I_full=30 А ⇒ **G = 1.45/(0.066·30) = 0.73** (неинвертирующий ≤1
реализуется делителем после ОУ либо выбираем ACS712-05B с G=4.4 при малом моторе).
Midrail НЕ берётся из расчёта — **измеряется при старте** (`current_sense_calibrate_midrail`),
так как ACS712 ratiometric и любое отклонение 3.3A видно сразу. Окно RMS = целое число
полупериодов сети (регион 50/60 уже в config) — математика в win_len().

## 3. Пин-карта v1 (devkit сейчас → кастом позже; hal_board.h — источник истины)

| Сигнал | GPIO | Блок | Комментарий |
|---|---|---|---|
| CT_CURRENT | 1 | ADC1_CH0 | только ADC1! analog |
| AC_ZC_IN | 2 | GPIO-ISR/HINF | компаратор LM393 с opto-входа, <1 В порог |
| TRIAC_FWD (MOC3052 LED−) | 3 via ULN2803 | open-collect sink | 330Ω gate, random-phase |
| TRIAC_REV | 4 | | |
| RELAY_K1/K2/K3 | 5/6/7 | H-bridge driver ULN2803 | flyback 1N4148, coil from +12 |
| ENC_A/B (optical/DX/DI) | 8/9 (custom: PCNT0 pair) | GPIO-ISR на devkit | strap-safe |
| SW_CLOSED/SW_OPEN | 10/11 | in, pull-up 10k + 1N4148 cell | dry contact |
| BTN_OPEN/STOP/CLOSE | 12/13/14 | + ESD TVS SMBJ6.5CA | |
| BEEP | 15 | LEDC square | piezo/buzzer via transistor |
| LAMP_TRIAC | 16 | MOC3052 ch.3 | BT136 + RC |
| IR_ILLUM (850nm LED) | 17 | MOS FDD8798 | включается синхронно с wake камеры |
| CAM_* (OV5640 DVP) | 18..21,23,24,36..41,39,40,42 | see hal_board | XCLK PWM-timer, PWDN enable-controlled |
| MAINS_OK_SENSE | 45?? NO — strap! → 47 | rectifier+divider or opto | ext0 wake on loss |
| BATT_VSENSE | 48 | ADC1_CH7 (via 3.3V tap) | INA219-I2C alt (SDA4, SCL5 custom) |
| CHRG_STAT / THERM_INT | 6/21 shared by board rev | charge IC flag | thermal interlock hook |
| Debug UART | TX43/RX44 | USB serial-JTAG fallback | release-build gated |
| BOOT/RESET | pads | NOT routed to header | strap pins 0,45,46 untouched |

(Точные номера фиксируются при layout-ревью; таблица — контракт прошивки.)

## 4. Питание v1 (дерево)
220 В → F1 5A T → MOV S14K275 + CX1 X2 → **T1 220/2×12 В 15 ВА** (toroid, −40 °C rated)
→ D1 bridge → Cbulk 470µF/35 **tantalum/polymer** → MP1584 buck → 3.3 В digital (ESP32, relays coils via 12 В branch)
                                   → LP5907/LDL low-noise LDO → 3.3A analog (ACS712, OU, ADC ref)
Battery 12 В LiFePO4 → ideal diode LTC4015-style OR-ing → charger TP5100/BQ25690 with NTC interlock
→ holdup caps 2×220 µF/25 polymer across 12 В rail (holdup calc in power_mgmt docs).
LDO 5 В legacy убран entirely; всё 3.3 В логика + 12 В реле-катушки.

## 5. Чек-лист приёмки платы
[ ] Hi-pot 3 кВ primary→logic; creepage ≥6.4 мм (IPC-2 class 3, conformal coat)
[ ] ZC-фронт: <100 мкс jitter на 45–65 Гц (scope into GPIO2)
[ ] CT: injected 0.5/5/20 A load → measured ±3 %; midrail drift <±15 LSB at −20/+55 °C
[ ] Phase-angle: firing delay error <±50 мкс @0–180° both regions (50 & 60 Hz bench)
[ ] Relay dead-time scope-measured ≥60 мс fwd-rev switching
[ ] Loss-of-mains capture: position saved within 40 мс at −20 °C cold caps
[ ] SNAPSHOT: all straps float-free at boot; board boots with any button held
