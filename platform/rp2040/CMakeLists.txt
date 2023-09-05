#This is the top-level cmake file to configure and build firmware for RP2040 based
#microcontroller boards.

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

#since a cmake project can only be configured for a single toolchain target
#the project is named by the toolchain (rp2040 here)
#this also lists the used languages for the project. These are taken from the pico-sdk.
project(rp2040 C CXX ASM)

#initializing the Pi Pico SDK must be done after declaring the project
pico_sdk_init()

message("\n\n-- Configuring CTI RP2040 build --\n")

#These defaults are used if cmake if this cmake is invoked without specifying
#CTI_PLATFORM or CTI_BOARD as a commandline option
set(DEFAULT_CTI_PLATFORM rp2040)
set(DEFAULT_CTI_BOARD pi_pico)

#bring in all the common CTI cmake functionality
include(../common/cti.cmake)

#bring in the pi pico source and build config
include(rp2040.inc.cmake)

message("\n-- Configuring firmware--\n")

# We expect RP2040_STDIO to be set to 'usb', 'uart', or 'none'
# If not defined we default to usb.
# Can be set to uart to allow using usb port for other tasks
# and requires setting additional RP2040_STDIO_UART_XX values below
if (NOT DEFINED RP2040_STDIO)
    set(RP2040_STDIO "usb")
    message("No STDIO mapping specified, defaulting to usb")
endif ()

message("STDIO configured over ${RP2040_STDIO}")

#from the above common cti.cmake, creates an executable build spec
#with the Pi Pico, both VISA and uDAQ modes are built in together
#so the executable is prefixed as such. ("visa-udaq")
# VISA_TARGET and VISA_OUTPUT are output variables from the command
create_cti_build("visa" VISA_TARGET VISA_OUTPUT)

target_sources(${VISA_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/platform/pi_pico_visa.cpp
)

#enable visa firmware mode for this firmware
target_compile_definitions(${VISA_TARGET}
    PUBLIC CTI_VISA=1
)
configure_rp2040(${VISA_TARGET})
message("CTI-Visa output will be ${VISA_OUTPUT}.uf2")