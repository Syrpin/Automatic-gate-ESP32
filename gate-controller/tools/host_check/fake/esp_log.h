#pragma once
#include <stdio.h>
typedef int esp_log_level_t;
#define ESP_LOG_ERROR 0
#define ESP_LOG_WARN 1
#define ESP_LOG_INFO 2
#define ESP_LOG_DEBUG 3
#define ESP_LOG_VERBOSE 4
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "I %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) do { if (0) fprintf(stderr, fmt "\n", ##__VA_ARGS__); } while (0)
#define ESP_LOGV(tag, fmt, ...) do { if (0) fprintf(stderr, fmt "\n", ##__VA_ARGS__); } while (0)

#include <stdlib.h>
#define ESP_ERROR_CHECK(x) do { esp_err_t rc_=(x); (void)rc_; } while(0)
