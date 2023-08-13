set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#determine CTI source root
get_filename_component(CTI_SOURCE_DIR "../../source" ABSOLUTE)
get_filename_component(CTI_INCLUDE_DIR "../../include" ABSOLUTE)

if (NOT CTI_PLATFORM)
    message("No CTI_PLATFORM set, defaulting to ${DEFAULT_CTI_PLATFORM}")
    set(CTI_PLATFORM ${DEFAULT_CTI_PLATFORM})
endif()

if(NOT CTI_BOARD)
    message("No CTI_BOARD set, defaulting to ${DEFAULT_CTI_BOARD}")
    set(CTI_BOARD ${DEFAULT_CTI_BOARD})
endif()

if(NOT CTI_BOARD_DIR)
    set(CTI_BOARD_DIR ${CMAKE_SOURCE_DIR}/boards/${CTI_BOARD})
endif()
if(NOT EXISTS ${CTI_BOARD_DIR}/cti_board.cmake)
    message(FATAL_ERROR "Invalid CTI_BOARD specified: ${CTI_BOARD}, no cti_board.cmake found in ${CTI_BOARD_DIR}")
endif()

#Board cmake pulls in configs such as pinouts and supported peripherals
include(${CTI_BOARD_DIR}/cti_board.cmake)

#include default config of features for main source
include (${CMAKE_CURRENT_LIST_DIR}/cti_features.cmake)

#include build config of main source
include (${CTI_SOURCE_DIR}/cti.cmake)

#brings in the variables that contain the firmware version
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)

set(BUILD_HEADER "${CTI_INCLUDE_DIR}/cti/build.h")
set(BUILD_CACHE "${CTI_INCLUDE_DIR}/cti/version_build.txt")

get_filename_component(BUILD_INC_PATH "${CMAKE_CURRENT_LIST_DIR}/build_timestamp.cmake" ABSOLUTE)

set(CTI_VERSION "${CTI_VER_MAJOR}.${CTI_VER_MINOR}.${CTI_VER_PATCH}")

#Creates a dependency target that handles updating the version within the firmware and can be depended on by firmware targets to get run before the firmwar build.
add_custom_target(cti_Version)
add_custom_command(
    TARGET cti_Version
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND}
        -P "${BUILD_INC_PATH}"
)

function(create_cti_build prefix target output)
    set(CTI_TARGET "cti_${prefix}_${CTI_PLATFORM}_${CTI_BOARD}")
    set(CTI_OUTPUT "${CTI_TARGET}_${CTI_VERSION}")

    set(${target} "${CTI_TARGET}" PARENT_SCOPE)
    set(${output} "${CTI_OUTPUT}" PARENT_SCOPE)

    add_executable(${CTI_TARGET})
    set_target_properties(${CTI_TARGET} PROPERTIES OUTPUT_NAME "${CTI_OUTPUT}")

    target_compile_definitions(${CTI_TARGET}
        PUBLIC _CTI_PLATFORM=${CTI_PLATFORM}
        PUBLIC    _CTI_BOARD=${CTI_BOARD}
        PUBLIC CTI_VER_MAJOR=${CTI_VER_MAJOR}
        PUBLIC CTI_VER_MINOR=${CTI_VER_MINOR}
        PUBLIC CTI_VER_PATH=${CTI_VER_PATCH}
    )

    target_sources(${CTI_TARGET} PUBLIC
        ${CTI_SOURCE_CORE}
        ${CTI_SOURCE_PLATFORM}
    )

    target_include_directories(${CTI_TARGET}
        PUBLIC ${CTI_INCLUDE}
    )

    add_dependencies(${CTI_TARGET} cti_Version)
endfunction()