#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_err.h"

/* attributes / macros */
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_DATA_ATTR
#define EXT_RAM_ATTR
#define BIT(n) (1UL << (n))

/* freertos basics */
typedef void *TaskHandle_t;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
typedef void *EventGroupHandle_t;
typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
typedef unsigned int UBaseType_t;
typedef void (*TaskFunction_t)(void *);
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define configMAX_PRIORITIES 25

static inline BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack,
                                     void *arg, UBaseType_t prio, TaskHandle_t *out) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; (void)out; return pdPASS;
}
static inline BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack,
                                                 void *arg, UBaseType_t prio, TaskHandle_t *out, int core) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; (void)out; (void)core; return pdPASS;
}
static inline void vTaskDelay(TickType_t t) { (void)t; }
static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline TaskHandle_t xTaskGetCurrentTaskHandle(void) { return NULL; }
static inline void vTaskDelete(TaskHandle_t t) { (void)t; }

/* critical sections */
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
static inline void vPortEnterCritical(void) {}
static inline void vPortExitCritical(void) {}
#define portENTER_CRITICAL(m) vPortEnterCritical()
#define portEXIT_CRITICAL(m) vPortExitCritical()
#define portENTER_CRITICAL_ISR(m) vPortEnterCritical()
#define portEXIT_CRITICAL_ISR(m) vPortExitCritical()
#define portYIELD_FROM_ISR()
#define portYIELD()

/* queues (minimal) */
static inline QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t size) { (void)len; (void)size; return (QueueHandle_t)1; }
static inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t t) { (void)q; (void)item; (void)t; return pdTRUE; }
static inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *hw) { (void)q; (void)item; (void)hw; return pdTRUE; }
static inline BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t t) { (void)q; (void)item; (void)t; return pdFALSE; }

/* semaphores (minimal) */
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) { return (SemaphoreHandle_t)1; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s; (void)t; return pdTRUE; }

static inline uint32_t xPortGetFreeHeapSize(void) { return 0; }
static inline uint32_t xPortGetMinimumEverFreeHeapSize(void) { return 0; }
