cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS "encoder.c"
                       INCLUDE_DIRS "include"
                       REQUIRES hal_board storage_nvs motor_drv esp_timer)
