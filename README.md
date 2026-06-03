# Stitch Counter

A stitch counter for knitting/crochet running on a Raspberry Pi Pico (RP2040)
with a 128×160 ST7735 SPI TFT display, a rotary encoder, and two side buttons.
Written in C against the Pico SDK.

> ## ⚠️ Disclaimer
>
> **This entire repository was written by AI** (Claude) with a human in the
> loop driving the hardware decisions and acceptance testing. It works on the
> author's specific build, but your mileage may vary — wiring, board revisions,
> SDK versions, and toolchain quirks can all shift things around. Treat the
> code as a starting point, not a polished product, and expect to debug.

## What it does

This device is used to count things. It was made to count rows while knitting,
but it can count anything. The screen shows the current **count** and a
**target**. The count is persisted to flash, so it survives sleep and
power-off.

### Controls

- **+ button** — increment the count by one.
- **− button** — decrement the count by one (won't go below zero).
- **Rotary encoder (turn)** — adjust the target up or down.
- **Encoder short press** — reset the count to zero.
- **Encoder long press** — save state and enter dormant sleep. Press the
  encoder again to wake.
- **Auto-sleep** — after a stretch of inactivity, the device dims and then
  goes to dormant sleep on its own.

### Easter egg: Thelma

Hold the increment and decrement buttons down for 5 seconds
and the counter screen is replaced by a tiny sheep on a patch of grass,
this is Thelma, the sheep. While in Tamagotchi mode the buttons do the following:

- **+ button** — scratch the sheep (it gets happy, hearts pop out).
- **− button** — feed the sheep (it munches, crumbs scatter).
- **Encoder short press** — ask the sheep to do a trick (it hops).
- **Rotary encoder (turn)** — walk the sheep left/right across the screen.
- **Encoder long press** — exit pet mode and go to sleep.
- **Inactivity** — after a few seconds with no input, pet mode auto-exits
  back to the counter.

### Flash-mode combo

Hold **+, −, and the encoder button all at once for 5 seconds** and the
device reboots into the RP2040 USB bootloader (BOOTSEL mode).
Handy for re-flashing without having to physically reach the BOOTSEL button.

## Project layout

```
src/
  main.c           Boot + main loop, callbacks wiring modules together
  state.c/h        Persistent count/target — written to last flash sector with CRC
  display.c/h      ST7735 driver (hand-rolled), screen layouts
  gfx.c/h          Drawing primitives and text rendering
  font8x8.h        Embedded 8×8 bitmap font (printable ASCII)
  encoder.c/h      IRQ-driven rotary encoder + short/long press detection
  buttons.c/h      Debounced +/- buttons
  power.c/h        PWM backlight, auto-dim, dormant sleep with GPIO wake
  pet.c/h          On-screen pet animation
tools/png2c.py     Helper to convert PNG sprites to C headers
CMakeLists.txt
pico_sdk_import.cmake
pico_extras_import.cmake
```

## Hardware

- Raspberry Pi Pico (RP2040)
- ST7735 128×160 SPI TFT
- Rotary encoder with push switch
- Two momentary buttons (+/−)

### Wiring / pin map

All GPIO numbers are RP2040 GPIO (the `GPxx` labels on the Pico pinout, not
the physical header pin numbers).

| Peripheral             | Signal            | Pico pin   |
|------------------------|-------------------|----------  |
| ST7735 TFT             | SCK               | GP18       |
|                        | SDA / MOSI        | GP19       |
|                        | RST               | GP20       |
|                        | A0 / DC           | GP21       |
|                        | CS                | GP17       |
|                        | LED+ (backlight)  | GP22 (PWM) |
|                        | LED−              | GND        |
|                        | VCC               | 3V3        |
|                        | GND               | GND        |
| Rotary encoder         | CLK               | GP10       |
|                        | DT                | GP11       |
|                        | SW (push switch)  | GP15       |
|                        | + / GND           | 3V3 / GND  |
| `+` button             | one leg           | GP4        |
|                        | other leg         | GND        |
| `−` button             | one leg           | GP3        |
|                        | other leg         | GND        |
| Battery (optional)     | switched +        | VSYS       |
|                        | −                 | GND        |

## Prerequisites

You need an ARM embedded toolchain, CMake, and the Pico SDK plus pico-extras
(the latter for `pico/sleep.h`, which the dormant-sleep path uses).

### macOS (Homebrew)

```bash
brew install --cask gcc-arm-embedded   # or: brew install arm-none-eabi-gcc
brew install cmake picotool
```

### Linux (Debian/Ubuntu)

```bash
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 libstdc++-arm-none-eabi-newlib build-essential
# picotool: build from source — https://github.com/raspberrypi/picotool
```

### Pico SDK + pico-extras

Clone both somewhere convenient (any location works — you'll point at them
with environment variables):

```bash
git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
git clone --recurse-submodules https://github.com/raspberrypi/pico-extras.git
```

## Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras

mkdir -p build && cd build
cmake -DPICO_BOARD=pico ..
make -j
```

Output: `build/polly_counter.uf2`.

## Flash

Two options.

**1. BOOTSEL drag-and-drop**

Hold the BOOTSEL button on the Pico while plugging in USB. It mounts as a USB
drive named `RPI-RP2`. Copy `build/polly_counter.uf2` to it. The Pico reboots
into the new firmware.

**2. picotool**

```bash
picotool load -fx build/polly_counter.uf2
```

`-f` forces reboot into BOOTSEL via USB; `-x` runs the program after loading.
Requires the device to already be running firmware that exposes the USB reset
interface, or already in BOOTSEL. From a cold state, hold BOOTSEL on plug-in
for the first load.

## Notes / gotchas

- **Sleep:** uses `pico/sleep` (from `pico-extras`) —
  `sleep_run_from_xosc()` → `sleep_goto_dormant_until_pin(GP15, edge, low)`.
  On wake, we re-init clocks, display, and PWM backlight.
- **Display backlight pin (GP22) and PWM:** GP22 is on PWM slice 3 channel A.
  Verified in `power_init` via the SDK's `pwm_gpio_to_slice_num` helper.
- **Font:** the embedded 8×8 font covers printable ASCII (0x20..0x7F).
  Non-printable characters render as `?`.
