
cmake_minimum_required(VERSION 3.21)

## Disable in-source build.
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source build is not allowed! Please specify a build folder.\n\tex:cmake -B build")
endif()

## Project declaration
project(mic_aggregator)

## Enable languages for project
enable_language(CXX C ASM)

## Add top level project targets
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(app_mic_aggregator)
endif()

add_subdirectory(fwk_io)
add_subdirectory(lib_xcore_math)

