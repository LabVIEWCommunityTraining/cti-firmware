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

message("\n-- Configuring VISA_UDAQ firmware--\n")

#from the above common cti.cmake, creates an executable build spec
#with the Pi Pico, both VISA and uDAQ modes are built in together
#so the executable is prefixed as such. ("visa-udaq")
# VISA_UDAQ_TARGET and VISA_UDAQ_OUTPUT are output variables from the command
create_cti_build("visa-udaq" VISA_UDAQ_TARGET VISA_UDAQ_OUTPUT)

#create_cti_build will set the specific VISA_UDAQ_OUTPUT variable to the filename without the extension
message("Output will be ${VISA_UDAQ_OUTPUT}.uf2")

#tell the pico-sdk to generate the additional Pi Pico output files (uf2)
#for the created executable build spec target
pico_add_extra_outputs(${VISA_UDAQ_TARGET})

# We expect VISA_UDAQ_STDIO to be set to 'usb', 'uart', or 'none'
# If not defined we default to usb.
# Can be set to uart to allow using usb port for other tasks
# and requires setting additional VISA_UDAQ_STDIO_UART_XX values below
if (NOT DEFINED VISA_UDAQ_STDIO)
    set(VISA_UDAQ_STDIO "usb")
    message("No STDIO mapping specified, defaulting to usb")
endif ()

message("STDIO configured over ${VISA_UDAQ_STDIO}")

#Sets the driver to use for stdio
if (VISA_UDAQ_STDIO STREQUAL "usb")
    pico_enable_stdio_usb(${VISA_UDAQ_TARGET} 1)
    pico_enable_stdio_uart(${VISA_UDAQ_TARGET} 0)
elseif (VISA_UDAQ_STDIO STREQUAL "uart")

    # The following VIREO_STDIO_UART_XX values configure the port when pico_stdio_uart is used above
    # Default is pins on the bottom right of the Pico pinout diagram, similar to small arduino board RX/TX layout
    if (NOT DEFINED VISA_UDAQ_STDIO_UART_PORT)
        set(VISA_UDAQ_STDIO_UART_PORT uart0)
    endif ()

    if (NOT DEFINED VISA_UDAQ_STDIO_UART_BAUD)
        set(VISA_UDAQ_STDIO_UART_BAUD 115200)
    endif ()

    # Note: RX and TX values can be set to -1 to disable that stdio direction
    if (NOT DEFINED VISA_UDAQ_STDIO_UART_RX)
        set(VISA_UDAQ_STDIO_UART_RX 21)
    endif()

    if (NOT DEFINED VISA_UDAQ_STDIO_UART_TX)
        set(VISA_UDAQ_STDIO_UART_TX 22)
    endif ()
endif ()

#Add the source files specific to rp2040 to the build
target_sources(${VISA_UDAQ_TARGET} PRIVATE
    platform/pi_pico_platform.cpp
    platform/pi_pico_io.cpp
    platform/pi_pico_timer.cpp
)

# These are the components we're using from the pico-sdk
# building a list variable to use with target_link_library command below.
set(PICO_SDK_COMPONENTS
    pico_stdlib
    hardware_i2c
)

target_link_libraries(${VISA_UDAQ_TARGET}
    ${PICO_SDK_COMPONENTS}
)

message("\n")