function (cti_configure_board CTI_BOARD_DIR TARGET)
    message("\n-- Configuring pico_w board for target ${TARGET} --")
    target_sources(${TARGET} PUBLIC
        ${CTI_BOARD_DIR}/pi_pico_w_status_led.cpp
    )

    target_include_directories(${TARGET} PUBLIC
        ${CTI_BOARD_DIR}/include
    )

    target_link_libraries(${TARGET}
        pico_cyw43_arch_lwip_poll
    )
endfunction()