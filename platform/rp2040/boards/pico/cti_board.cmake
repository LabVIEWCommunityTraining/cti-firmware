function (cti_configure_board target)
    target_sources(${target} PUBLIC
        pi_pico_status_led.cpp
    )
endfunction()