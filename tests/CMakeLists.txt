cmake_minimum_required(VERSION 3.20)
project(cti_host_tests LANGUAGES CXX)
enable_testing()
set(CTI_HUB75_WIDTH 64 CACHE STRING "HUB75 test columns")
set(CTI_HUB75_HEIGHT 64 CACHE STRING "HUB75 test rows")
add_executable(cti_tests
    test_main.cpp
    ../source/scpi/scpi_parser.cpp
    ../source/scpi/scpi_node.cpp
    ../source/scpi/scpi_error_queue.cpp
    ../platform/rp2350/platform/hub75.cpp
)
target_compile_features(cti_tests PRIVATE cxx_std_17)
target_compile_definitions(cti_tests PRIVATE CTI_HUB75=1
    CTI_HUB75_WIDTH=${CTI_HUB75_WIDTH} CTI_HUB75_HEIGHT=${CTI_HUB75_HEIGHT})
target_include_directories(cti_tests PRIVATE fakes ../include)
if(MSVC)
    target_compile_options(cti_tests PRIVATE /W4 /UNDEBUG)
else()
    target_compile_options(cti_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME parser_frame_lifecycle COMMAND cti_tests)
