// Stitch Counter — main.c
// Raspberry Pi Pico (RP2040) + 128x160 ST7735 SPI TFT
//
// Wiring summary:
//   TFT GND  → GND       TFT VCC → 3V3
//   TFT SCK  → GP18      TFT SDA → GP19
//   TFT RST  → GP20      TFT AO  → GP21
//   TFT CS   → GP17      TFT LED+→ GP22 (PWM), LED- → GND
//   ENC CLK  → GP10      ENC DT  → GP11
//   ENC SW   → GP12      ENC SW  → GP15 (bridged, wake pin)
//   ENC +    → 3V3       ENC GND → GND
//   INC btn  → GP4 + GND   (moved from GP2 — GP2 damaged)
//   DEC btn  → GP3 + GND
//   Battery  → VSYS (Pin 39) via switch

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"

#include "state.h"
#include "display.h"
#include "gfx.h"
#include "power.h"
#include "encoder.h"
#include "buttons.h"
#include "pet.h"

#define COMBO_HOLD_MS 5000
#define FLASH_COMBO_HOLD_MS 5000

// Pins polled directly for the flash-mode combo. Already initialized as
// pulled-up inputs by buttons_init() / encoder_init().
#define PIN_INC_RAW 4
#define PIN_DEC_RAW 3
#define PIN_ENC_SW  15

typedef enum {
    MODE_COUNTER,
    MODE_PET,
} app_mode_t;

static app_mode_t s_mode = MODE_COUNTER;

// ── Counter-mode callbacks ──────────────────────────────────────────────────

static void counter_on_rotate(int delta) {
    int32_t cur = (int32_t)state_get_target();
    int32_t next = cur + delta;
    if (next < 0) next = 0;
    state_set_target((uint32_t)next);
    display_update_target(state_get_count(), state_get_target());
    power_notify_activity();
}

static void counter_on_short_press(void) {
    state_reset_count();
    display_update_count(0, state_get_target());
    power_notify_activity();
}

static void enter_sleep(void) {
    display_show_message("Saving...", "", COLOR_YELLOW);
    state_save();
    sleep_ms(400);
    display_show_message("Sleeping.", "Press to wake", COLOR_GRAY);
    sleep_ms(600);

    power_go_dormant();

    // Resumed — debounce wake press, re-init display, redraw.
    sleep_ms(300);
    power_init();
    display_init();
    gfx_init();
    display_draw_screen(state_get_count(), state_get_target());
}

static void counter_on_long_press(void) {
    enter_sleep();
}

static void counter_on_increment(void) {
    state_increment();
    display_update_count(state_get_count(), state_get_target());
    power_notify_activity();
}

static void counter_on_decrement(void) {
    state_decrement();
    display_update_count(state_get_count(), state_get_target());
    power_notify_activity();
}

// ── Dispatch wrappers (route to counter or pet based on mode) ───────────────

static void on_rotate(int delta) {
    power_notify_activity();
    if (s_mode == MODE_PET) pet_on_rotate(delta);
    else                    counter_on_rotate(delta);
}

static void on_short_press(void) {
    power_notify_activity();
    if (s_mode == MODE_PET) pet_on_trick();
    else                    counter_on_short_press();
}

static void on_long_press(void) {
    // If INC + DEC are also held, the user is going for the flash-mode combo
    // (3 buttons for 5s). Don't sleep — let the combo timer keep counting.
    if (gpio_get(PIN_INC_RAW) == 0 && gpio_get(PIN_DEC_RAW) == 0) return;

    // Long-press always exits pet mode (if active) and goes to sleep.
    if (s_mode == MODE_PET) s_mode = MODE_COUNTER;
    counter_on_long_press();
}

static void on_increment(void) {
    power_notify_activity();
    if (s_mode == MODE_PET) pet_on_scratch();
    else                    counter_on_increment();
}

static void on_decrement(void) {
    power_notify_activity();
    if (s_mode == MODE_PET) pet_on_feed();
    else                    counter_on_decrement();
}

// ── Easter-egg trigger ──────────────────────────────────────────────────────

static void on_combo_held(void) {
    if (s_mode == MODE_PET) return;
    s_mode = MODE_PET;
    pet_enter();
}

// ── Flash-mode combo: INC + DEC + encoder SW held for 5s ────────────────────

static void check_flash_mode_combo(void) {
    static uint32_t hold_start_ms = 0;
    static bool     armed         = false;

    bool all_down = (gpio_get(PIN_INC_RAW) == 0)
                 && (gpio_get(PIN_DEC_RAW) == 0)
                 && (gpio_get(PIN_ENC_SW)  == 0);

    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!all_down) {
        armed = false;
        return;
    }

    if (!armed) {
        armed = true;
        hold_start_ms = now;
        return;
    }

    if ((now - hold_start_ms) >= FLASH_COMBO_HOLD_MS) {
        display_show_message("Flash mode", "Reboot to BOOTSEL", COLOR_YELLOW);
        sleep_ms(500);
        // Reboot directly into the USB mass-storage bootloader.
        // 0,0 = no activity LED, all interfaces enabled.
        reset_usb_boot(0, 0);
        // Unreachable.
    }
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(void) {
    power_init();
    display_init();
    gfx_init();

    state_load();
    display_draw_screen(state_get_count(), state_get_target());

    encoder_init(on_rotate, on_short_press, on_long_press);
    buttons_init(on_increment, on_decrement);
    buttons_set_combo_handler(on_combo_held, COMBO_HOLD_MS);

    while (true) {
        check_flash_mode_combo();
        encoder_tick();
        buttons_tick();
        state_tick();
        power_tick();

        if (s_mode == MODE_PET) {
            pet_tick();
            // Keep activity timer fresh so auto-sleep never fires in pet mode.
            power_notify_activity();
            if (!pet_is_active()) {
                // Auto-exit → redraw counter screen.
                s_mode = MODE_COUNTER;
                display_draw_screen(state_get_count(), state_get_target());
            }
        } else {
            display_tick();
            if (power_should_auto_sleep()) {
                enter_sleep();
            }
        }

        sleep_ms(5);
    }
}
