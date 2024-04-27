get_filename_component(CTI_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../include" ABSOLUTE)

include (${CMAKE_CURRENT_LIST_DIR}/platform/platform.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/scpi/scpi.cmake)
include (${CMAKE_CURRENT_LIST_DIR}/visa/visa.cmake)

set(CTI_SOURCE_CORE
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)