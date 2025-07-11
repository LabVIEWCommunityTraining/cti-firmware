function (cti_configure_board CTI_BOARD_DIR TARGET)
    message("\n-- Configuring pico_w board for target ${TARGET} --")
    target_sources(${TARGET} PUBLIC
        ${CTI_BOARD_DIR}/pico_w.cpp
        ${CTI_BOARD_DIR}/pi_pico_w_status_led.cpp
        ${CTI_BOARD_DIR}/pi_pico_wifi.cpp
        ${CTI_BOARD_DIR}/../pico/pi_pico_stdio.cpp
        ${CTI_BOARD_DIR}/../../../../source/visa/wifi.cpp
    )

    target_include_directories(${TARGET} PUBLIC
        ${CTI_BOARD_DIR}/include
    )

    target_link_libraries(${TARGET}
        pico_cyw43_arch_lwip_poll
        hardware_flash
    )

    target_compile_definitions(${TARGET} PUBLIC
        CTI_NUM_STREAMS=2
        CTI_FEATURE_WIFI=1
        CTI_FEATURE_DEBUG=1
        CTI_DEBUG_STREAM=0
    )
endfunction()