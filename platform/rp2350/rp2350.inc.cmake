set(RP2350_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})

function(configure_rp2350 RP2350_TARGET)
    target_sources(${RP2350_TARGET} PRIVATE
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_platform.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_io.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_timer.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_pwm.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_visa.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_comms.cpp
    )

    target_link_libraries(${RP2350_TARGET}
        pico_stdlib
        pico_unique_id
        hardware_uart
        hardware_i2c
        hardware_adc
        hardware_pwm
        hardware_clocks
        hardware_spi
    )

    pico_add_extra_outputs(${RP2350_TARGET})
endfunction()
