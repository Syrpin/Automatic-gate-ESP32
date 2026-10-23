idf_component_register(SRCS "sim.c"
                       INCLUDE_DIRS "include"
                       REQUIRES gate_logic motor_drv encoder limit_sw buttons
                                current_sense power_mgmt storage_nvs events_log
                                hal_board esp_timer console esp_console)
