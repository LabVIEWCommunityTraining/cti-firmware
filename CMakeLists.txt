# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

set(CtiProject "CtiFirmware")

#These defaults are used if cmake if this cmake is invoked without specifying
#CTI_PLATFORM or CTI_BOARD as a commandline option
set(DEFAULT_CTI_PLATFORM rp2040)
set(DEFAULT_CTI_BOARD ${PICO_BOARD})

include(platform/common/cti.cmake)
include(platform/${PICO_PLATFORM}/${PICO_PLATFORM}.inc.cmake)

project(${CtiProject} C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

#from the above common cti.cmake, creates an executable build spec
#with the Pi Pico, both VISA and uDAQ modes are built in together
#so the executable is prefixed as such. ("visa-udaq")
# VISA_TARGET and VISA_OUTPUT are output variables from the command
create_cti_build("visa" VISA_TARGET VISA_OUTPUT)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${VISA_TARGET} 0)
pico_enable_stdio_usb(${VISA_TARGET} 1)

# Add the standard include files to the build
target_include_directories(${VISA_TARGET} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

#enable visa firmware mode for this firmware
target_compile_definitions(${VISA_TARGET}
    PUBLIC CTI_VISA=1
)
configure_rp2040(${VISA_TARGET})
message("CTI-Visa output will be ${VISA_OUTPUT}.uf2")
