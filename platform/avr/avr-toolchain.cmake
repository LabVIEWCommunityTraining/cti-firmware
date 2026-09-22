set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(AVR_MCU "atmega328p" CACHE STRING "AVR microcontroller")
set(AVR_TOOLCHAIN_ROOT "" CACHE PATH "Root of an avr-gcc toolchain")

if(NOT AVR_TOOLCHAIN_ROOT AND DEFINED ENV{AVR_TOOLCHAIN_ROOT})
    file(TO_CMAKE_PATH "$ENV{AVR_TOOLCHAIN_ROOT}" AVR_TOOLCHAIN_ROOT)
endif()

if(NOT AVR_TOOLCHAIN_ROOT AND WIN32 AND DEFINED ENV{LOCALAPPDATA})
    file(GLOB arduino_avr_toolchains
        "$ENV{LOCALAPPDATA}/Arduino15/packages/arduino/tools/avr-gcc/*")
    list(SORT arduino_avr_toolchains COMPARE NATURAL ORDER DESCENDING)
    list(LENGTH arduino_avr_toolchains arduino_avr_toolchain_count)
    if(arduino_avr_toolchain_count GREATER 0)
        list(GET arduino_avr_toolchains 0 AVR_TOOLCHAIN_ROOT)
    endif()
endif()

find_program(AVR_CC avr-gcc HINTS "${AVR_TOOLCHAIN_ROOT}/bin")
find_program(AVR_CXX avr-g++ HINTS "${AVR_TOOLCHAIN_ROOT}/bin")
find_program(AVR_AR avr-ar HINTS "${AVR_TOOLCHAIN_ROOT}/bin")
find_program(AVR_RANLIB avr-ranlib HINTS "${AVR_TOOLCHAIN_ROOT}/bin")
find_program(AVR_OBJCOPY avr-objcopy HINTS "${AVR_TOOLCHAIN_ROOT}/bin")
find_program(AVR_OBJDUMP avr-objdump HINTS "${AVR_TOOLCHAIN_ROOT}/bin")
find_program(AVR_SIZE avr-size HINTS "${AVR_TOOLCHAIN_ROOT}/bin")

if(NOT AVR_CC OR NOT AVR_CXX OR NOT AVR_AR OR NOT AVR_RANLIB OR
   NOT AVR_OBJCOPY OR NOT AVR_OBJDUMP OR NOT AVR_SIZE)
    message(FATAL_ERROR
        "AVR GCC tools were not found. Set AVR_TOOLCHAIN_ROOT to a pinned avr-gcc "
        "installation (the directory containing bin/avr-gcc).")
endif()

set(CMAKE_C_COMPILER "${AVR_CC}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${AVR_CXX}" CACHE FILEPATH "" FORCE)
set(CMAKE_AR "${AVR_AR}" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${AVR_RANLIB}" CACHE FILEPATH "" FORCE)

message(STATUS "Using AVR toolchain: ${AVR_TOOLCHAIN_ROOT}")
