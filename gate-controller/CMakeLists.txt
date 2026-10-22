# ESP-IDF project root for gate-controller (STM32F103 -> ESP32-S3 N16R8 port)
# Build: idf.py set-target esp32s3 && idf.py build
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gate_controller)
