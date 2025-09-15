cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w)

IF ( NOT DEFINED ENV{PICO_SDK_PATH})
    message( FATAL_ERROR "Please setup the pico-sdk and set the PICO_SDK_PATH environment variable")
ENDIF()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(web-host-mcu C CXX ASM)

include(${CMAKE_CURRENT_LIST_DIR}/build.cmake)
