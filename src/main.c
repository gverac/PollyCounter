// PollyCounter — main.c
// Multi-program firmware: counter, rep counter, countdown, stopwatch,
// settings, plus a menu and the Thelma easter egg.

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

#include "program.h"
#include "counter.h"
#include "rep.h"
#include "countdown.h"
#include "stopwatch.h"
#include "settings.h"
#include "menu.h"

#define COMBO_HOLD_MS       5000
#define FLASH_COMBO_HOLD_MS 5000

#define PIN_INC_RAW 4
#define PIN_DEC_RAW 3
#define PIN_ENC_SW  15

// ── Program registry (indexed by program_id_t) ──────────────────────────────

static const program_t *PROGRAMS[PROGRAM__COUNT];

static void init_registry(void) {
    PROGRAMS[PROGRAM_COUNTER]   = &COUNTER_PROGRAM;
    PROGRAMS[PROGRAM_REP]       = &REP_PROGRAM;
    PROGRAMS[PROGRAM_COUNTDOWN] = &COUNTDOWN_PROGRAM;
    PROGRAMS[PROGRAM_STOPWATCH] = &STOPWATCH_PROGRAM;
    PROGRAMS[PROGRAM_SETTINGS]  = &SETTINGS_PROGRAM;
}

// ── Current program / menu state ────────────────────────────────────────────

static const program_t *s_current  = NULL;   // active program (or menu)
static const program_t *s_previous = NULL;   // program to restore when menu closes
static bool             s_in_menu  = false;
static bool             s_in_pet   = false;
static bool             s_flash_confirm = false;   // showing flash-mode confirm
static uint32_t         s_flash_combo_start_ms = 0;

static void enter_program(const program_t *p) {
    if (s_current && s_current->on_exit) s_current->on_exit();
    s_current = p;
    if (s_current && s_current->on_enter) s_current->on_enter();
}

// ── Sleep ──────────────────────────────────────────────────────────────────

static void enter_sleep(void) {
    display_show_message("Saving...", "", COLOR_YELLOW);
    state_save();
    sleep_ms(400);
    display_show_message("Sleeping.", "Press to wake", COLOR_GRAY);
    sleep_ms(600);

    power_go_dormant();

    // Woken — re-init hardware and redraw current program.
    sleep_ms(300);
    power_init();
    settings_apply_to_power();
    display_init();
    gfx_init();

    if (s_in_menu) {
        if (MENU_PROGRAM.draw) MENU_PROGRAM.draw();
    } else if (s_current && s_current->draw) {
        s_current->draw();
    }
}

// ── App control surface (used by menu) ──────────────────────────────────────

void app_switch_to(program_id_t id) {
    if (id >= PROGRAM__COUNT) return;
    s_in_menu = false;
    s_previous = NULL;
    state_set_current_program(id);
    enter_program(PROGRAMS[id]);
}

void app_open_menu(void) {
    if (s_in_menu) return;
    s_previous = s_current;
    s_in_menu  = true;
    menu_reset_cursor();
    enter_program(&MENU_PROGRAM);
}

void app_close_menu(void) {
    if (!s_in_menu) return;
    s_in_menu = false;
    const program_t *back = s_previous ? s_previous : PROGRAMS[PROGRAM_COUNTER];
    s_previous = NULL;
    enter_program(back);
}

void app_request_sleep(void) {
    enter_sleep();
}

static void draw_flash_confirm(void) {
    display_show_message("FLASH MODE?", "Hold + and -", COLOR_YELLOW);
}

void app_request_flash_mode(void) {
    s_flash_confirm = true;
    s_flash_combo_start_ms = 0;
    draw_flash_confirm();
}

static void cancel_flash_confirm(void) {
    s_flash_confirm = false;
    if (s_current && s_current->draw) s_current->draw();
}

static void do_flash_mode(void) {
    display_show_message("Flash mode", "Reboot to BOOTSEL", COLOR_YELLOW);
    sleep_ms(500);
    reset_usb_boot(0, 0);
}

#define FLASH_CONFIRM_HOLD_MS 800

static void check_flash_confirm(void) {
    if (!s_flash_confirm) return;
    bool both = (gpio_get(PIN_INC_RAW) == 0) && (gpio_get(PIN_DEC_RAW) == 0);
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (!both) { s_flash_combo_start_ms = 0; return; }
    if (s_flash_combo_start_ms == 0) { s_flash_combo_start_ms = now; return; }
    if ((now - s_flash_combo_start_ms) >= FLASH_CONFIRM_HOLD_MS) {
        do_flash_mode();
    }
}

// ── Input dispatch ──────────────────────────────────────────────────────────

static void dispatch_rotate(int delta) {
    power_notify_activity();
    if (s_flash_confirm) { cancel_flash_confirm(); return; }
    if (s_in_pet)  { pet_on_rotate(delta); return; }
    if (s_current && s_current->on_rotate) s_current->on_rotate(delta);
}

static void dispatch_short_press(void) {
    power_notify_activity();
    if (s_flash_confirm) { cancel_flash_confirm(); return; }
    if (s_in_pet)  { pet_on_trick(); return; }
    if (s_current && s_current->on_short_press) s_current->on_short_press();
}

static void dispatch_long_press(void) {
    power_notify_activity();

    // If INC + DEC are also held, the user is going for the flash-mode combo.
    if (gpio_get(PIN_INC_RAW) == 0 && gpio_get(PIN_DEC_RAW) == 0) return;

    if (s_flash_confirm) { cancel_flash_confirm(); return; }

    if (s_in_pet) {
        // Exit pet mode and redraw whatever program was active. Don't propagate.
        s_in_pet = false;
        if (s_current && s_current->draw) s_current->draw();
        return;
    }

    if (s_current && s_current->on_long_press) {
        s_current->on_long_press();
        return;
    }
    // Default: open menu.
    app_open_menu();
}

static void dispatch_double_press(void) {
    power_notify_activity();
    if (s_in_pet) { s_in_pet = false; }

    if (s_current && s_current->on_double_press) {
        s_current->on_double_press();
        return;
    }
    // Default: sleep immediately.
    enter_sleep();
}

static void dispatch_increment(void) {
    power_notify_activity();
    if (s_flash_confirm) return;   // handled by check_flash_confirm (held combo)
    if (s_in_pet)  { pet_on_scratch(); return; }
    if (s_current && s_current->on_increment) s_current->on_increment();
}

static void dispatch_decrement(void) {
    power_notify_activity();
    if (s_flash_confirm) return;
    if (s_in_pet)  { pet_on_feed(); return; }
    if (s_current && s_current->on_decrement) s_current->on_decrement();
}

// ── Easter egg ──────────────────────────────────────────────────────────────

static void on_combo_held(void) {
    if (s_in_pet) return;
    s_in_pet = true;
    pet_enter();
}

// ── Flash-mode 3-button combo (kept as backup) ──────────────────────────────

static void check_flash_mode_combo(void) {
    static uint32_t hold_start_ms = 0;
    static bool     armed         = false;

    bool all_down = (gpio_get(PIN_INC_RAW) == 0)
                 && (gpio_get(PIN_DEC_RAW) == 0)
                 && (gpio_get(PIN_ENC_SW)  == 0);

    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!all_down) { armed = false; return; }

    if (!armed) {
        armed = true;
        hold_start_ms = now;
        return;
    }

    if ((now - hold_start_ms) >= FLASH_COMBO_HOLD_MS) {
        app_request_flash_mode();
    }
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(void) {
    power_init();
    display_init();
    gfx_init();

    state_load();
    settings_apply_to_power();

    init_registry();

    encoder_init(dispatch_rotate, dispatch_short_press, dispatch_long_press);
    encoder_set_double_press(dispatch_double_press);
    buttons_init(dispatch_increment, dispatch_decrement);
    buttons_set_combo_handler(on_combo_held, COMBO_HOLD_MS);

    // Boot into the last-used program.
    program_id_t boot_id = state_get_current_program();
    enter_program(PROGRAMS[boot_id]);

    while (true) {
        check_flash_mode_combo();
        check_flash_confirm();
        encoder_tick();
        buttons_tick();
        state_tick();
        power_tick();

        if (s_in_pet) {
            pet_tick();
            // Keep activity timer fresh so auto-sleep never fires in pet mode.
            power_notify_activity();
            if (!pet_is_active()) {
                s_in_pet = false;
                if (s_current && s_current->draw) s_current->draw();
            }
        } else {
            if (s_current && s_current->tick) s_current->tick();
            if (power_should_auto_sleep()) {
                enter_sleep();
            }
        }

        sleep_ms(5);
    }
}
