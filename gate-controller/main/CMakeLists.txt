cmake_minimum_required(VERSION 3.16)
set(extra_req "")
if(CONFIG_GATE_SIM)
  list(APPEND extra_req sim)
endif()
if(CONFIG_WEBUI_ENABLE)
  list(APPEND extra_req webui)
endif()
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES gate_logic hal_board storage_nvs power_mgmt events_log ${extra_req})
