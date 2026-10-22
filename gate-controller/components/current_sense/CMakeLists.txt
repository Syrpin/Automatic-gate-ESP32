cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS "current_sense.c" INCLUDE_DIRS "include" REQUIRES hal_board storage_nvs esp_adc)
