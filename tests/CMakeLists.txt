cmake_minimum_required(VERSION 3.20)
project(cti_host_tests LANGUAGES CXX)
enable_testing()
set(CTI_HUB75_WIDTH 64 CACHE STRING "HUB75 test columns")
set(CTI_HUB75_HEIGHT 64 CACHE STRING "HUB75 test rows")
foreach(TEST_TARGET IN ITEMS cti_tests cti_tests_rp2040)
add_executable(${TEST_TARGET}
    test_main.cpp
    test_ws2811.cpp
    ../source/scpi/scpi_parser.cpp
    ../source/scpi/scpi_node.cpp
    ../source/scpi/scpi_error_queue.cpp
    ../platform/common/hub75.cpp
    ../platform/common/ws2811.cpp
)
target_compile_features(${TEST_TARGET} PRIVATE cxx_std_17)
target_compile_definitions(${TEST_TARGET} PRIVATE CTI_HUB75=1 CTI_WS2811=1
    CTI_HUB75_WIDTH=${CTI_HUB75_WIDTH} CTI_HUB75_HEIGHT=${CTI_HUB75_HEIGHT})
target_include_directories(${TEST_TARGET} PRIVATE fakes ../include)
if(MSVC)
    target_compile_options(${TEST_TARGET} PRIVATE /W4 /UNDEBUG)
else()
    target_compile_options(${TEST_TARGET} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME ${TEST_TARGET}_parser_frame_lifecycle COMMAND ${TEST_TARGET})
endforeach()
target_compile_definitions(cti_tests_rp2040 PRIVATE PICO_RP2040=1)
