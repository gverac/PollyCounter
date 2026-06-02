cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(stitch_counter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(stitch_counter
    main.c
    state.c
    display.c
    gfx.c
    encoder.c
    buttons.c
    power.c
    pet.c
)

target_link_libraries(stitch_counter
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
pico_add_extra_outputs(stitch_counter)

# Disable USB & UART stdio (we don't need a console; saves binary size)
pico_enable_stdio_usb(stitch_counter 0)
pico_enable_stdio_uart(stitch_counter 0)
