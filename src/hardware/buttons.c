#include "buttons.h"

#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define PIN_INC 4
#define PIN_DEC 3
#define DEBOUNCE_MS 50

typedef struct {
    uint            pin;
    button_press_fn cb;
    int             last;
    uint32_t        pressed_ms;
    bool            suppress_next_release;
} button_t;

static button_t s_inc = {PIN_INC, NULL, 1, 0, false};
static button_t s_dec = {PIN_DEC, NULL, 1, 0, false};

// Combo (both held)
static button_press_fn s_combo_cb      = NULL;
static uint32_t        s_combo_thresh  = 5000;
static bool            s_combo_active  = false;   // both currently pressed
static uint32_t        s_combo_start   = 0;
static bool            s_combo_fired   = false;   // already fired this hold

static void btn_setup(button_t *b) {
    gpio_init(b->pin);
    gpio_set_dir(b->pin, GPIO_IN);
    gpio_pull_up(b->pin);
    b->last = gpio_get(b->pin);
}

static void btn_tick(button_t *b) {
    int val = gpio_get(b->pin);
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (b->last == 1 && val == 0) {
        // Press
        b->pressed_ms = now;
    } else if (b->last == 0 && val == 1) {
        // Release
        bool fire = (now - b->pressed_ms) >= DEBOUNCE_MS
                    && b->cb != NULL
                    && !b->suppress_next_release;
        b->suppress_next_release = false;
        if (fire) b->cb();
    }
    b->last = val;
}

void buttons_init(button_press_fn on_increment, button_press_fn on_decrement) {
    s_inc.cb = on_increment;
    s_dec.cb = on_decrement;
    btn_setup(&s_inc);
    btn_setup(&s_dec);
}

void buttons_set_combo_handler(button_press_fn on_combo, uint32_t threshold_ms) {
    s_combo_cb     = on_combo;
    s_combo_thresh = threshold_ms;
}

void buttons_tick(void) {
    // Combo detection BEFORE per-button tick, so we can suppress releases.
    bool inc_pressed = (gpio_get(s_inc.pin) == 0);
    bool dec_pressed = (gpio_get(s_dec.pin) == 0);
    bool both        = inc_pressed && dec_pressed;
    uint32_t now     = to_ms_since_boot(get_absolute_time());

    if (both) {
        // Any moment of both-held → suppress whichever release happens next on
        // each button. Prevents an accidental double-tap firing inc + dec.
        s_inc.suppress_next_release = true;
        s_dec.suppress_next_release = true;

        if (!s_combo_active) {
            s_combo_active = true;
            s_combo_start  = now;
            s_combo_fired  = false;
        } else if (!s_combo_fired && (now - s_combo_start) >= s_combo_thresh) {
            s_combo_fired = true;
            if (s_combo_cb) s_combo_cb();
        }
    } else {
        s_combo_active = false;
    }

    btn_tick(&s_inc);
    btn_tick(&s_dec);
}
