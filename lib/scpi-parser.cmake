# cmake include for getting scpi-parser built into the cti firmware.

# We're not relying on the makefiles and built library already prepared
# for scpi-parser so that we can more easily integrate with the build
# tooling being setup for the rest of CTI and handling different
# toolchains.

set(SCPI_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/scpi-parser/libscpi/src)
set(SCPI_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/scpi-parser/libscpi/inc)

message("SCPI_INCLUDE_DIR is ${SCPI_INCLUDE_DIR}")

set(SCPI_PARSER_SOURCE
    ${SCPI_SOURCE_DIR}/error.c
    ${SCPI_SOURCE_DIR}/expression.c
    ${SCPI_SOURCE_DIR}/fifo.c
    ${SCPI_SOURCE_DIR}/ieee488.c
    ${SCPI_SOURCE_DIR}/lexer.c
    ${SCPI_SOURCE_DIR}/minimal.c
    ${SCPI_SOURCE_DIR}/parser.c
    ${SCPI_SOURCE_DIR}/units.c
    ${SCPI_SOURCE_DIR}/utils.c
)