cmake_minimum_required(VERSION 3.13)

# Emit build/compile_commands.json so clangd-based editors (VSCode + the
# clangd extension, Neovim, etc.) can resolve Pico SDK headers and defines.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(polly_counter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(polly_counter
    src/main.c
    src/state.c

    src/hardware/encoder.c
    src/hardware/buttons.c
    src/hardware/power.c

    src/graphics/display.c
    src/graphics/gfx.c

    src/util/menu.c

    src/programs/counter.c
    src/programs/rep.c
    src/programs/tally.c
    src/programs/countdown.c
    src/programs/stopwatch.c
    src/programs/interval.c
    src/programs/settings.c
    src/programs/pet.c
)

# Flat include paths — each module can `#include "display.h"` etc. without
# caring which subdirectory the header lives in.
target_include_directories(polly_counter PRIVATE
    src
    src/hardware
    src/graphics
    src/util
    src/programs
)

target_link_libraries(polly_counter
    pico_stdlib
    hardware_spi
    hardware_pwm
    hardware_flash
    hardware_sync
    hardware_irq
    hardware_gpio
    hardware_dma
    hardware_sleep
    hardware_rosc
    hardware_rtc
)

# Generate .uf2/.elf/.bin/.hex
pico_add_extra_outputs(polly_counter)

# Disable USB & UART stdio (we don't need a console; saves binary size)
pico_enable_stdio_usb(polly_counter 0)
pico_enable_stdio_uart(polly_counter 0)
