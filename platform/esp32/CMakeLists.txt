cmake_minimum_required(VERSION 3.20)

set(CTI_PLATFORM esp32 CACHE STRING "" FORCE)
if(NOT DEFINED CTI_BOARD OR CTI_BOARD STREQUAL "")
    set(CTI_BOARD htit-wb32la CACHE STRING "CTI target board" FORCE)
endif()
set_property(CACHE CTI_BOARD PROPERTY STRINGS wishio-lora-915 htit-wb32la)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH must point to an ESP-IDF checkout")
endif()

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(CtiFirmware)
