# PollyCounter

A pocket(ish)-sized multi-program counter and timer running on a Raspberry Pi Pico
(RP2040) with a 128×160 ST7735 SPI TFT display, a rotary encoder, and two side
buttons. Written in C against the Pico SDK.

It started life as a stitch counter for knitting/crochet — that mode is still
the default — but it now also runs a rep counter, a countdown timer, a
stopwatch, a settings screen, and a sheep tamagotchi easter egg.

> ## ⚠️ Disclaimer
>
> **This entire repository was written by AI** (Claude) with a human in the
> loop driving the hardware decisions and acceptance testing. It works on the
> author's specific build, but your mileage may vary — wiring, board revisions,
> SDK versions, and toolchain quirks can all shift things around. Treat the
> code as a starting point, not a polished product, and expect to debug.

## 3D Models

You can find STLs and STEP files for this project in Printables [here](https://www.printables.com/model/1742448-polly-counter-a-knitting-counter).

## Programs

PollyCounter is organised as a small set of selectable **programs**. The device
boots back into whichever program was active last. Persistent state (counts,
targets, configured durations, settings) survives sleep and power-off.

### Global controls

These work in every program:

- **Encoder long press** — open the program **menu**.
- **Encoder double press** — save state and enter dormant sleep immediately.
  Press the encoder again to wake. (Since "Sleep" is the top entry in the
  menu, *long press → short press* also sleeps.)
- **Hold + and − for 5 seconds** — summon Thelma, the sheep (easter egg).
- **Auto-sleep** — after a stretch of inactivity the screen dims, then the
  device goes dormant on its own. Both timeouts are configurable in Settings.

### Menu

Encoder rotate to scroll, short press to select, long press to close.

The entries are: **Sleep · Counter · Rep Counter · Multi-tally · Countdown ·
Stopwatch · Interval · Settings · Flash mode**.

### Counter

The classic stitch-counter screen. Shows the current **count** and a **target**;
hitting the target plays a small fireworks celebration.

- **+ button** — increment.
- **− button** — decrement (won't go below zero).
- **Rotary encoder (turn)** — adjust the target.
- **Encoder short press** — reset the count to zero.

### Rep Counter

Tracks **reps** within a **set**. On entry you set the targets first, then the
device drops into a live counting screen.

Setup:

- Rotate to set the **target reps**, encoder short press to confirm.
- Rotate to set the **target sets**, encoder short press to confirm.

Live counting:

- **+ button** — add a rep. On reaching the target, sets++ and reps reset to 0.
- **− button** — remove a rep. If reps are already 0 and there's a completed
  set, roll back: sets-- and reps = target_reps − 1.
- **Encoder short press** — reset reps & sets to 0 (targets are preserved).
  To change the targets, exit to the menu and re-enter the program.

### Multi-tally

Four independent counters (A / B / C / D) shown in a 2×2 grid. Each one
persists independently to flash. Handy when you're tracking several things at
once — sets and reps, score in a card game, two pets fed, etc.

- **Rotary encoder (turn)** — pick the active cell (peach border).
- **+ button** — increment the active counter.
- **− button** — decrement the active counter (won't go below zero).
- **Encoder short press** — reset the active counter to zero. Other cells are
  untouched.

### Countdown

A simple count-down timer (mm:ss) that can pause and resume.

Setup:

- Rotate to adjust the active digit pair. A blinking peach underline shows
  which one you're editing.
- **Encoder short press** — toggle between **MIN** and **SEC**.
- **+ button** — start the timer.

Running / paused:

- **− button** — pause. **+** or **−** resumes.
- **Encoder short press** — go back to setup (re-edit minutes/seconds).

When the timer reaches 0 it shows a **DONE!** screen indefinitely until you:

- **+ button** — restart immediately with the same time.
- **− button** — dismiss (returns to a "READY" idle screen; + starts again).
- **Encoder short press** — re-enter setup.

### Stopwatch

A mm:ss.cs stopwatch.

- **+ button** — start / stop.
- **− button** — reset to 0 (only while stopped).
- **Encoder short press** — record a lap (shown below while running).

### Interval

A Tabata-style interval timer with configurable **work**, **rest**, and
**round count**. Auto-cycles work → rest → work → … with a colored phase
indicator (mint = work, pink = rest, yellow = paused).

Setup:

- Encoder short press cycles through **WORK · REST · ROUNDS**. A blinking
  peach underline shows the active field.
- Rotate to adjust the active field (WORK and REST are mm:ss, ROUNDS is 1–99).
  REST can be 0 for back-to-back work intervals.
- **+ button** — start.

Running:

- The round counter ("3/8") sits above the timer.
- **− button** — pause; **+** or **−** resumes.
- **Encoder short press** — re-enter setup.

When all rounds finish, a **DONE!** screen waits for input:

- **+ button** — restart with the same settings.
- **− button** — dismiss (returns to setup).

### Settings

A four-row screen edited with the encoder.

- Rotate to move the cursor between rows.
- **Encoder short press** — enter edit mode on the highlighted row (turns it
  peach); rotate to change the value; short press again to commit.
- **− button** — cancel an edit, or cancel the reset confirmation.

Rows:

- **BRIGHT** — backlight brightness (5–100 %).
- **SLEEP** — auto-sleep timeout (30 s – 1 h).
- **DIM** — auto-dim timeout (5 s – 10 min).
- **RESET** — wipe all persisted state back to defaults. Asks for confirmation:
  encoder short press to confirm, − to cancel.

### Flash mode

Reboots into the RP2040 USB bootloader (BOOTSEL mode) so you can drag-and-drop
a fresh `.uf2`. To avoid accidental triggers there is an explicit confirmation:

- Either select **Flash mode** from the menu **or** hold **+ + − +
  encoder** together for 5 seconds.
- The screen shows `FLASH MODE? — Hold + and −`. Holding **+** and **−**
  together for ~1 second reboots into BOOTSEL.
- Any other input (encoder rotate or press) cancels.

### Easter egg: Thelma

Hold **+** and **−** down together for 5 seconds, from any program, and
the screen is replaced by a tiny sheep on a patch of grass — Thelma. While in
pet mode:

- **+ button** — scratch the sheep (it gets happy, hearts pop out).
- **− button** — feed the sheep (it munches, crumbs scatter).
- **Encoder short press** — ask the sheep to do a trick (it hops).
- **Rotary encoder (turn)** — walk the sheep left/right across the screen.
- **Encoder long press** — exit pet mode, return to the previous program.
- **Inactivity** — after a few seconds with no input, pet mode auto-exits.

## Project layout

```
src/
  main.c             Boot + main loop, program dispatch, sleep & flash-mode flow
  program.h          Vtable interface implemented by every program
  state.c/h          Versioned persistent record — last flash sector with CRC.
                     Migrates from the original single-counter layout.

  hardware/          Peripheral drivers
    encoder.c/h      IRQ-driven rotary encoder + short/long/double-press
    buttons.c/h      Debounced +/- buttons + simultaneous-hold combo
    power.c/h        PWM backlight, configurable auto-dim, dormant sleep

  graphics/          Display stack
    display.c/h      ST7735 driver (hand-rolled) + shared text/screen helpers
    gfx.c/h          Framebuffer + drawing primitives
    font8x8.h        Embedded 8×8 bitmap font (printable ASCII)

  util/              Non-program glue
    menu.c/h         Program selection menu

  programs/          Each entry in the menu
    counter.c/h      Counter (original behavior)
    rep.c/h          Rep counter (reps + sets, with setup phase)
    tally.c/h        Multi-tally — 4 independent counters in a 2×2 grid
    countdown.c/h    Countdown timer (min/sec edit, pause/resume, DONE screen)
    stopwatch.c/h    Stopwatch (start/stop, lap, reset)
    interval.c/h     Interval / Tabata timer (work / rest / rounds)
    settings.c/h     Brightness / sleep / dim / reset-all
    pet.c/h          Thelma (sheep tamagotchi easter egg)

tools/png2c.py       Helper to convert PNG sprites to C headers
CMakeLists.txt
pico_sdk_import.cmake
pico_extras_import.cmake
```

Headers are flat on the include path (`src/`, `src/hardware/`,
`src/graphics/`, `src/util/`, `src/programs/`), so any module can write
`#include "display.h"` without caring which directory the header lives in.

## Hardware

- Raspberry Pi Pico (RP2040). A Pico W also works since this firmware doesn't
  use the wireless chip.
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

Output: `build/polly_counter.uf2`. The same `.uf2` runs on a Pico W as well.

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
