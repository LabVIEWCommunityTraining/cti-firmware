cmake_minimum_required(VERSION 3.13)

message("\n-- Initializing Pi Pico SDK --")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

#Get PICO_SDK_PATH from environment variable if set, otherwise look for pico-sdk submodule.
if (DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
else()
    get_filename_component(PICO_SDK_PATH "./pico-sdk" ABSOLUTE)
    if (NOT EXISTS ${PICO_SDK_PATH}/pico_sdk_init.cmake)
        message(FATAL_ERROR "PICO_SDK_PATH variable not set in environment and pico-sdk could not be found. Ensure the pico-sdk submodule has been initialized.")
    endif()
endif()

#make the pico-sdk available to the rest of the build system
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(rp2040 C CXX ASM)
pico_sdk_init()

message("\n\n-- Configuring CTI RP2040 build --\n")

set(DEFAULT_CTI_PLATFORM rp2040)
set(DEFAULT_CTI_BOARD pi_pico)

include(../common/cti.cmake)