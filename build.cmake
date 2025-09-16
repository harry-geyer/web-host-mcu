set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_compile_options(-Wall
    -Werror
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function # we have some for the docs that aren't called
    -Wno-unused-parameter
    -Wno-maybe-uninitialized
    -Wno-address-of-packed-member
    -Wall
    -Wextra
    -Wformat=2
    -Wcast-align
    -Wwrite-strings
    -Wunreachable-code
    -Wstrict-aliasing=2
    -ffloat-store
    -fno-common
    -fstrict-aliasing
    -Wno-format-nonliteral
)

add_executable(firmware
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/dhcp_server.c
)

target_link_libraries(firmware
    pico_stdlib
)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(firmware pico_cyw43_arch_lwip_poll)
endif()

pico_add_extra_outputs(firmware)
pico_enable_stdio_usb(firmware 1)

execute_process(
    COMMAND
        ${BASH}
        git describe --tags --abbrev=0 --dirty
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

IF (GIT_TAG STREQUAL "")
    set(GIT_TAG "0.0-dirty")
ENDIF()

execute_process(
    COMMAND
        ${BASH}
        git log -n 1 --format="%h"
    OUTPUT_VARIABLE GIT_SHA1
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(FIRMWARE_NAME="${CMAKE_PROJECT_NAME}"
    FIRMWARE_VERSION="${GIT_TAG}"
    FIRMWARE_SHA1=${GIT_SHA1})

IF (NOT DEFINED CONFIG_DIR)
    set(CONFIG_DIR ${CMAKE_CURRENT_LIST_DIR}/include/config)
ENDIF ()

target_include_directories(firmware PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/internal
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CONFIG_DIR}
)

add_custom_target(flashpico
    COMMAND
        ${BASH}
        /bin/bash
        ${CMAKE_CURRENT_LIST_DIR}/tools/program.sh "$<TARGET_FILE:firmware>"
    COMMENT "Flashing PICO"
)

get_target_property(FIRMWARE_SOURCES firmware SOURCES)

add_custom_target(cppcheck
    COMMAND
        ${BASH}
        set -x &&
        cd ${CMAKE_CURRENT_LIST_DIR} &&
        cppcheck --check-level=exhaustive --std=c11 `ls ${FIRMWARE_SOURCES} | xargs realpath`
    COMMENT "cppchecking"
)

add_custom_target(openocd
    COMMAND
        ${BASH}
        openocd -f interface/cmsis-dap.cfg -c "adapter speed 5000" -f target/rp2040.cfg -s tcl
    COMMENT "Starting debug"
)

add_custom_target(gdb
    COMMAND
        ${BASH}
        gdb-multiarch -ex "target extended-remote localhost:3333" firmware.elf
    COMMENT "Attaching"
)

add_dependencies(flashpico firmware)
