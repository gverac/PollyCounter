cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(polly_counter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(polly_counter
    src/main.c
    src/state.c
    src/display.c
    src/gfx.c
    src/encoder.c
    src/buttons.c
    src/power.c
    src/pet.c
)

target_include_directories(polly_counter PRIVATE src)

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
