function (cti_configure_board target)
    target_sources(${target} PUBLIC
        ${CTI_BOARD_DIR}/pi_pico.cpp
        ${CTI_BOARD_DIR}/pi_pico_status_led.cpp
        ${CTI_BOARD_DIR}/pi_pico_stdio.cpp
    )

    target_compile_definitions(${TARGET} PUBLIC
        CTI_NUM_STREAMS=1
    )
endfunction()