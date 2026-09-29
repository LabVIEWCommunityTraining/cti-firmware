cmake_minimum_required(VERSION 3.20)

set(CTI_PLATFORM avr CACHE STRING "" FORCE)
set(CTI_BOARD arduino-uno CACHE STRING "")
set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type")

include(avr-toolchain.cmake)
project(CtiFirmwareAvr C CXX)

set(DEFAULT_CTI_PLATFORM avr)
set(DEFAULT_CTI_BOARD arduino-uno)

include(../common/cti.cmake)
include(avr.inc.cmake)

create_cti_build("visa" VISA_TARGET VISA_OUTPUT)
target_compile_definitions(${VISA_TARGET} PUBLIC CTI_VISA=1)
configure_avr(${VISA_TARGET})
