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

add_executable(application
    ${CMAKE_CURRENT_LIST_DIR}/src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/src/dhcp_server.c
    ${CMAKE_CURRENT_LIST_DIR}/src/http_server.c
    ${CMAKE_CURRENT_LIST_DIR}/src/config.c
    ${CMAKE_CURRENT_LIST_DIR}/src/htu31d.c
    ${CMAKE_CURRENT_LIST_DIR}/libs/tiny-json/tiny-json.c
)

target_link_libraries(application
    pico_stdlib
    pico_lwip_http
    pico_httpd_webroot
    pico_cyw43_arch_lwip_poll
    hardware_i2c
)

pico_add_library(pico_httpd_webroot NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_webroot INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/webroot/index.html
    ${CMAKE_CURRENT_LIST_DIR}/webroot/app.js
    ${CMAKE_CURRENT_LIST_DIR}/webroot/styles.css
)

set_target_properties(application
    PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/application.ld
)

pico_add_extra_outputs(application)
pico_enable_stdio_usb(application 1)

IF (NOT DEFINED CONFIG_DIR)
    set(CONFIG_DIR ${CMAKE_CURRENT_LIST_DIR}/include/config)
ENDIF ()

target_include_directories(application PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/src/internal
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/bootloader/include
    ${CMAKE_CURRENT_LIST_DIR}/libs/tiny-json
    ${CONFIG_DIR}
)

add_executable(bootloader
    ${CMAKE_CURRENT_LIST_DIR}/bootloader/src/main.c
)

target_link_libraries(bootloader
    pico_stdlib
    hardware_flash
)

set_target_properties(bootloader
    PROPERTIES PICO_TARGET_LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/bootloader/bootloader.ld
)

target_include_directories(bootloader PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/bootloader/include
    ${CONFIG_DIR}
)

pico_add_extra_outputs(bootloader)

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

add_custom_target(firmware ALL
    COMMAND
        ${BASH}
        dd if=bootloader.bin of=firmware.bin bs=256 &&
        dd if=application.bin of=firmware.bin bs=256 seek=160 &&
        chmod +x firmware.bin &&
        picotool uf2 convert firmware.bin firmware.uf2 --family rp2350-arm-s --abs-block
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Combining bootloader and application"
)

add_dependencies(firmware
    bootloader
    application
)

add_custom_target(flashpico
    COMMAND
        ${BASH}
        /bin/bash
        ${CMAKE_CURRENT_LIST_DIR}/tools/program.sh "$<TARGET_FILE:application>"
    COMMENT "Flashing PICO"
)

get_target_property(FIRMWARE_SOURCES application SOURCES)

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
        gdb-multiarch -ex "target extended-remote localhost:3333" application.elf
    COMMENT "Attaching"
)

add_dependencies(flashpico application)
