file(GLOB CTI_SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/../../../../source/platform/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../source/scpi/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../source/visa/*.cpp"
)

if(NOT DEFINED CTI_BOARD OR CTI_BOARD STREQUAL "")
    set(CTI_BOARD "htit-wb32la")
endif()

set(CTI_BOARD_DIR "${CMAKE_CURRENT_LIST_DIR}/../../boards/${CTI_BOARD}")
if(NOT EXISTS "${CTI_BOARD_DIR}/cti_board.h")
    message(FATAL_ERROR "CTI board directory not found for '${CTI_BOARD}': ${CTI_BOARD_DIR}")
endif()

file(GLOB CTI_BOARD_SOURCE_FILES "${CTI_BOARD_DIR}/*.cpp")
string(REPLACE "-" "_" CTI_BOARD_DEFINE "${CTI_BOARD}")

idf_component_register(
    SRCS
        "${CMAKE_CURRENT_LIST_DIR}/../../main.cpp"
        ${CTI_SOURCE_FILES}
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/esp32_platform.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/esp32_io.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/esp32_comms.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/esp32_pwm.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/esp32_timer.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../../platform/esp32_visa.cpp"
        ${CTI_BOARD_SOURCE_FILES}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_LIST_DIR}/../../../../include"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../include/cti"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../include/visa"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../include/scpi"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../platform/common/include"
        "${CMAKE_CURRENT_LIST_DIR}/../../../../source"
        "${CTI_BOARD_DIR}"
    REQUIRES
        driver
        esp_adc
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc
        esp_driver_spi
        esp_driver_uart
        esp_timer
        esp_system
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    CTI_VISA=1
    _CTI_PLATFORM=esp32
    _CTI_BOARD=${CTI_BOARD_DEFINE}
    CTI_VER_MAJOR=0
    CTI_VER_MINOR=9
    CTI_VER_PATCH=4
)
