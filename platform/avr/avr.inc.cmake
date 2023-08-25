

function (configure_avr AVR_TARGET)

#Add the source files specific to rp2040 to the build
target_sources(${AVR_TARGET} PRIVATE
    platform/avr_platform.cpp
    platform/avr_io.cpp
    platform/avr_timer.cpp
    src/avr_serial.cpp
    src/avr_timer.cpp
    src/circularbuffer.cpp
)

target_include_directories(${AVR_TARGET} PRIVATE
    get_filename_component(${CMAKE_CURRENT_SOURCE_DIR}/include ABSOLUTE)
)

target_compile_definitions(${AVR_TARGET} PRIVATE
    F_CPU=16000000ul
)

endfunction()