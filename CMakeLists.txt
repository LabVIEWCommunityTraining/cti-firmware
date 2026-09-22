cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CTI_PLATFORM "" CACHE STRING "CTI target platform (avr, rp2040, or rp2350)")
set_property(CACHE CTI_PLATFORM PROPERTY STRINGS avr rp2040 rp2350)

if(CTI_PLATFORM STREQUAL "avr")
    set(CTI_BOARD "arduino-uno" CACHE STRING "CTI target board")
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type")
    include(platform/avr/avr-toolchain.cmake)

    project(CtiFirmware C CXX)

    set(DEFAULT_CTI_PLATFORM avr)
    set(DEFAULT_CTI_BOARD arduino-uno)

    include(platform/common/cti.cmake)
    include(platform/avr/avr.inc.cmake)

    create_cti_build("visa" VISA_TARGET VISA_OUTPUT)
    target_compile_definitions(${VISA_TARGET} PUBLIC CTI_VISA=1)
    configure_avr(${VISA_TARGET})

    message(STATUS "CTI-Visa output will be ${CMAKE_BINARY_DIR}/${VISA_OUTPUT}.hex")
else()
    set(PICO_BOARD "pico_w" CACHE STRING "Pico SDK board type")
    set(CTI_BOARD "${PICO_BOARD}" CACHE STRING "CTI target board")

    if(WIN32)
        set(USERHOME $ENV{USERPROFILE})
    else()
        set(USERHOME $ENV{HOME})
    endif()
    set(sdkVersion 2.3.1)
    set(toolchainVersion 15_2_Rel1)
    set(picotoolVersion 2.3.1)
    set(picoVscode "${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake")
    if(EXISTS "${picoVscode}")
        include("${picoVscode}")
    endif()

    message("\n-- Including Pico SDK from ${PICO_SDK_PATH} --")
    include(pico_sdk_import.cmake)

    if(PICO_PLATFORM MATCHES "^rp2350")
        set(CTI_PLATFORM rp2350 CACHE STRING "CTI target platform" FORCE)
    else()
        set(CTI_PLATFORM rp2040 CACHE STRING "CTI target platform" FORCE)
    endif()

    project(CtiFirmware C CXX ASM)
    pico_sdk_init()

    set(DEFAULT_CTI_PLATFORM "${CTI_PLATFORM}")
    set(DEFAULT_CTI_BOARD "${PICO_BOARD}")

    include(platform/common/cti.cmake)
    include(platform/${CTI_PLATFORM}/${CTI_PLATFORM}.inc.cmake)

    create_cti_build("visa" VISA_TARGET VISA_OUTPUT)
    pico_enable_stdio_uart(${VISA_TARGET} 0)
    pico_enable_stdio_usb(${VISA_TARGET} 1)
    target_compile_definitions(${VISA_TARGET} PUBLIC CTI_VISA=1)
    cmake_language(CALL configure_${CTI_PLATFORM} ${VISA_TARGET})

    message(STATUS "CTI-Visa output will be ${VISA_OUTPUT}.uf2")
    add_custom_command(TARGET ${VISA_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/uf2"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${PROJECT_BINARY_DIR}/${VISA_OUTPUT}.uf2"
            "${PROJECT_SOURCE_DIR}/uf2/${VISA_OUTPUT}.uf2"
        COMMENT "Copying built firmware file to uf2 folder."
    )
endif()
