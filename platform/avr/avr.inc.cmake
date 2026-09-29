set(AVR_PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(configure_avr AVR_TARGET)
    set_target_properties(${AVR_TARGET} PROPERTIES SUFFIX ".elf")

    target_sources(${AVR_TARGET} PRIVATE
        "${AVR_PLATFORM_DIR}/platform/avr_platform.cpp"
        "${AVR_PLATFORM_DIR}/platform/avr_io.cpp"
        "${AVR_PLATFORM_DIR}/platform/avr_pwm.cpp"
        "${AVR_PLATFORM_DIR}/platform/avr_comms.cpp"
        "${AVR_PLATFORM_DIR}/platform/avr_timer.cpp"
        "${AVR_PLATFORM_DIR}/platform/avr_visa.cpp"
        "${AVR_PLATFORM_DIR}/src/avr_serial.cpp"
        "${AVR_PLATFORM_DIR}/src/avr_timer.cpp"
    )

    target_include_directories(${AVR_TARGET} PRIVATE
        "${AVR_PLATFORM_DIR}/include"
    )

    target_compile_definitions(${AVR_TARGET} PRIVATE
        F_CPU=16000000UL
        SCPI_INPUT_BUFFER_LENGTH=64
        SCPI_ERROR_QUEUE_SIZE=1
        SCPI_ERROR_STR_SIZE=24
        SCPI_ERR_BUFFER_SIZE=23
        SCPI_MAX_COMMANDS=36
        CTI_IO_BUFFER_LENGTH=16
        CTI_AVR_FIXED_REAL_OUTPUT=1
        CTI_SILENT_STARTUP=1
        CTI_MINIMAL_ERROR_STRINGS=1
        PlatformTickType=uint32_t
    )

    target_compile_options(${AVR_TARGET} PRIVATE
        -mmcu=${AVR_MCU}
        -Os
        -ffunction-sections
        -fdata-sections
        -fno-exceptions
        -fno-rtti
        -fno-threadsafe-statics
        -Wall
        -Wextra
        -Wno-unused-parameter
    )
    target_link_options(${AVR_TARGET} PRIVATE
        -mmcu=${AVR_MCU}
        -Wl,--gc-sections
        -Wl,--relax
        -Wl,-Map,${VISA_OUTPUT}.map
    )

    set(hex_file "${CMAKE_BINARY_DIR}/${VISA_OUTPUT}.hex")
    set(lst_file "${CMAKE_BINARY_DIR}/${VISA_OUTPUT}.lst")

    add_custom_command(TARGET ${AVR_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/hex"
        COMMAND "${AVR_OBJCOPY}" -j .text -j .data -O ihex
            "$<TARGET_FILE:${AVR_TARGET}>" "${hex_file}"
        COMMAND ${CMAKE_COMMAND} -E copy "${hex_file}"
            "${PROJECT_SOURCE_DIR}/hex/${VISA_OUTPUT}.hex"
        COMMAND "${AVR_OBJDUMP}" -h -S "$<TARGET_FILE:${AVR_TARGET}>" > "${lst_file}"
        COMMAND "${AVR_SIZE}" -C --mcu=${AVR_MCU} "$<TARGET_FILE:${AVR_TARGET}>"
        BYPRODUCTS "${hex_file}" "${lst_file}"
        COMMENT "Generating Arduino Uno HEX and memory report"
        VERBATIM
    )
endfunction()
