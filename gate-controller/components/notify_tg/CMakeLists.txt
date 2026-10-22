cmake_minimum_required(VERSION 3.16)
idf_component_register(SRCS "notify_tg.c" INCLUDE_DIRS "include" REQUIRES storage_nvs)
