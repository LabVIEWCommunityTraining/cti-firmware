cmake_minimum_required(VERSION 3.13)

message("\n-- Initializing AVR Toolchain --")

# Set build type to reduce firmware size
# Used by pico-sdk
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

include(avr-toolchain.cmake)

project (AVR C CXX)

message("\n\n-- Configuring CTI AVR build --\n")

include(avr.inc.cmake)

#These defaults are used if cmake if this cmake is invoked without specifying
#CTI_PLATFORM or CTI_BOARD as a commandline option
set(DEFAULT_CTI_PLATFORM avr)
set(DEFAULT_CTI_BOARD arduino-uno)

#bring in all the common CTI cmake functionality
include(../common/cti.cmake)

create_cti_build("visa" VISA_TARGET VISA_OUTPUT)

#enable visa firmware mode for this firmware
target_compile_definitions(${VISA_TARGET}
    PUBLIC CTI_VISA=1
)

configure_avr(${VISA_TARGET})
message("CTI-Visa output will be ${VISA_OUTPUT}.uf2")