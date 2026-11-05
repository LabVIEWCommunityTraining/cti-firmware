set(RP2350_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})
set(CTI_HUB75_WIDTH 64 CACHE STRING "HUB75 default columns (8..256; SCPI configurable)")
set(CTI_HUB75_HEIGHT 64 CACHE STRING "HUB75 default rows (16, 32 or 64; SCPI configurable)")
if(NOT CTI_HUB75_WIDTH MATCHES "^[1-9][0-9]*$" OR
   NOT CTI_HUB75_HEIGHT MATCHES "^(16|32|64)$")
    message(FATAL_ERROR "HUB75 requires an integer width 8..256 and height 16, 32 or 64")
endif()
if(CTI_HUB75_WIDTH LESS 8 OR CTI_HUB75_WIDTH GREATER 256)
    message(FATAL_ERROR "HUB75 width must be 8..256")
endif()

function(configure_rp2350 RP2350_TARGET)
    target_sources(${RP2350_TARGET} PRIVATE
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_platform.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_io.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_timer.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_pwm.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_visa.cpp
        ${RP2350_PLATFORM_DIR}/platform/pi_pico_comms.cpp
        ${RP2350_PLATFORM_DIR}/platform/hub75.cpp
    )
    target_compile_definitions(${RP2350_TARGET} PRIVATE CTI_HUB75=1 PICO_MALLOC_PANIC=0
        CTI_HUB75_WIDTH=${CTI_HUB75_WIDTH} CTI_HUB75_HEIGHT=${CTI_HUB75_HEIGHT})
    pico_generate_pio_header(${RP2350_TARGET} ${RP2350_PLATFORM_DIR}/hub75.pio)
    pico_set_program_description(${RP2350_TARGET}
        "CTI VISA with runtime HUB75 RGB332/RGB888 PIO/DMA; default ${CTI_HUB75_WIDTH}x${CTI_HUB75_HEIGHT}")

    target_link_libraries(${RP2350_TARGET}
        pico_stdlib
        pico_unique_id
        hardware_uart
        hardware_i2c
        hardware_adc
        hardware_pwm
        hardware_clocks
        hardware_spi
        hardware_pio
        hardware_dma
    )

    pico_add_extra_outputs(${RP2350_TARGET})
endfunction()
