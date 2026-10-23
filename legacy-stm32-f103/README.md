# Legacy: STM32F103 gate controller (original 2009-era firmware)

Source files of the original C project for STM32F103 (HAL/CubeMX): motor relay
control, ADC current protection, TIM encoder, buttons/beeper, CONDITION bitmask
state logic. Kept read-only as reference for the ESP32-S3 port.

Active development lives in `../gate-controller/` (ESP-IDF project).
Port plan and mapping old-code -> new-components: see `../ESP32_PORT_PLAN.md`.
