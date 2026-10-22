cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS "limit_sw.c" INCLUDE_DIRS "include" REQUIRES hal_board motor_drv esp_timer)
