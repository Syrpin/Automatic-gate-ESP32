idf_component_register(SRCS "sim.c"
                       INCLUDE_DIRS "include"
                       REQUIRES gate_logic motor_drv encoder limit_sw buttons
                                current_sense power_mgmt storage_nvs events_log
                                hal_board esp_timer esp_driver_gpio esp_console)
# sim.c implements the hooks declared in hal_board/include/sim_hooks.h; without
# this define that header compiles to inline stubs and would collide with the
# real definitions here.
target_compile_definitions(${COMPONENT_LIB} PRIVATE SIM_IMPLEMENTATION)
