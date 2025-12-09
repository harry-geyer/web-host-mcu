set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

find_program(TERSER terser)
find_program(CLEANCSS cleancss)
find_program(WEBPACK webpack-cli)

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
    ${CMAKE_CURRENT_LIST_DIR}/src/ap_station.c
    ${CMAKE_CURRENT_LIST_DIR}/src/common.c
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
    ${CMAKE_BINARY_DIR}/webroot/index.html
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/web/app.min.js
    COMMAND ${TERSER} ${CMAKE_SOURCE_DIR}/web/app.js --mangle --compress --output ${CMAKE_BINARY_DIR}/web/app.min.js
    DEPENDS ${CMAKE_SOURCE_DIR}/web/app.js
    COMMENT "Minifying JavaScript"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/web/styles.min.css
    COMMAND ${CLEANCSS} ${CMAKE_SOURCE_DIR}/web/styles.css -O2 -o ${CMAKE_BINARY_DIR}/web/styles.min.css
    DEPENDS ${CMAKE_SOURCE_DIR}/web/styles.css
    COMMENT "Minifying CSS"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/webroot/index.html
    COMMAND ${WEBPACK} --config ${CMAKE_SOURCE_DIR}/web/.webpack.config.js
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/web/index.html ${CMAKE_BINARY_DIR}/web/app.min.js ${CMAKE_BINARY_DIR}/web/styles.min.css
    COMMENT "Combining with HTML"
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
        openocd -s tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000"
    COMMENT "Starting debug"
)

add_custom_target(gdb
    COMMAND
        ${BASH}
        gdb-multiarch -ex "target extended-remote localhost:3333" application.elf
    DEPENDS application
    COMMENT "Attaching"
)

add_custom_target(flash
    COMMAND
        ${BASH}
        openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program application.elf verify reset exit"
    DEPENDS application
    COMMENT "Flashing"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
function(create_venv venv_dir requirements_path)
    if(EXISTS ${venv_dir})
        message(STATUS "Virtual environment already exists in ${venv_dir}, skipping creation.")
        return()
    endif()
    if(NOT EXISTS ${requirements_path})
        message(FATAL_ERROR "Requirements file not found: ${requirements_path}")
    endif()
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -m venv ${venv_dir}
        RESULT_VARIABLE venv_creation_ret_code
    )
    if(venv_creation_ret_code)
        message(FATAL_ERROR "Failed to create virtual environment at ${venv_dir}!")
    endif()
    execute_process(
        COMMAND ${venv_dir}/bin/pip install -r ${requirements_path}
        RESULT_VARIABLE pip_install_ret_code
    )
    if(pip_install_ret_code)
        message(FATAL_ERROR "Failed to install dependencies from ${requirements_path}!")
    endif()
    message(STATUS "Virtual environment setup done at ${venv_dir} with dependencies from ${requirements_path}")
endfunction()

set(HOST_VENV ${CMAKE_BINARY_DIR}/.host_venv)
create_venv(${HOST_VENV} ${CMAKE_SOURCE_DIR}/tools/fake_host/requirements.txt)

add_custom_target(fake_host
    COMMAND
        ${BASH}
        ${HOST_VENV}/bin/fastapi dev ${CMAKE_SOURCE_DIR}/tools/fake_host/main.py
    DEPENDS ${CMAKE_BINARY_DIR}/webroot/index.html
    COMMENT "Hosting"
)

add_dependencies(flashpico application)
