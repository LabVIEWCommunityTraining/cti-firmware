function (configure_rp2040 RP2040_TARGET)


#Sets the driver to use for stdio
if (RP2040_STDIO STREQUAL "usb")
    pico_enable_stdio_usb(${RP2040_TARGET} 1)
    pico_enable_stdio_uart(${RP2040_TARGET} 0)
elseif (RP2040_STDIO STREQUAL "uart")

    # The following VIREO_STDIO_UART_XX values configure the port when pico_stdio_uart is used above
    # Default is pins on the bottom right of the Pico pinout diagram, similar to small arduino board RX/TX layout
    if (NOT DEFINED RP2040_STDIO_UART_PORT)
        set(RP2040_STDIO_UART_PORT uart0)
    endif ()

    if (NOT DEFINED RP2040_STDIO_UART_BAUD)
        set(RP2040_STDIO_UART_BAUD 115200)
    endif ()

    # Note: RX and TX values can be set to -1 to disable that stdio direction
    if (NOT DEFINED RP2040_STDIO_UART_RX)
        set(RP2040_STDIO_UART_RX 21)
    endif()

    if (NOT DEFINED RP2040_STDIO_UART_TX)
        set(RP2040_STDIO_UART_TX 22)
    endif ()
endif ()

#Add the source files specific to rp2040 to the build
target_sources(${RP2040_TARGET} PRIVATE
    platform/pi_pico_platform.cpp
    platform/pi_pico_io.cpp
    platform/pi_pico_timer.cpp
    platform/pi_pico_pwm.cpp
)

# These are the components we're using from the pico-sdk
# building a list variable to use with target_link_library command below.
set(PICO_SDK_COMPONENTS
    pico_stdlib
    hardware_i2c
    hardware_adc
    hardware_pwm
    hardware_clocks
)

target_link_libraries(${RP2040_TARGET}
    ${PICO_SDK_COMPONENTS}
)

#tell the pico-sdk to generate the additional Pi Pico output files (uf2)
#for the created executable build spec target
pico_add_extra_outputs(${RP2040_TARGET})

endfunction()