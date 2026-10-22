cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS "power_mgmt.c" INCLUDE_DIRS "include"
                       REQUIRES hal_board storage_nvs events_log encoder esp_adc esp_timer)
