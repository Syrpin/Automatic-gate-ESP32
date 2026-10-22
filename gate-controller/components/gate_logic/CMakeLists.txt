cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS "gate_logic.c" INCLUDE_DIRS "include"
                       REQUIRES motor_drv encoder limit_sw buttons current_sense storage_nvs events_log power_mgmt hal_board esp_timer)
