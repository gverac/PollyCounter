#include "encoder.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#define PIN_CLK 10
#define PIN_DT  11
#define PIN_SW  15

#define LONG_PRESS_MS        800
#define DOUBLE_PRESS_MS      350

// Reject any CLK transitions less than this apart — pure contact-bounce rejection.
// A real detent takes 30-100ms to traverse mechanically, so 2000us is well clear
// of any legitimate edge while killing the bounce.
#define DEBOUNCE_US    2000

static encoder_rotate_fn s_on_rotate     = NULL;
static encoder_press_fn  s_on_short      = NULL;
static encoder_press_fn  s_on_long       = NULL;
static encoder_press_fn  s_on_double     = NULL;

static volatile int      s_pending_delta = 0;
static volatile int      s_last_clk      = 1;
static volatile uint32_t s_last_edge_us  = 0;

static int      s_sw_last     = 1;
static uint32_t s_press_ms    = 0;
static bool     s_long_fired  = false;

// Deferred-short-press state for double-press detection.
static bool     s_short_pending     = false;
static uint32_t s_short_pending_ms  = 0;

static void clk_irq(uint gpio, uint32_t events) {
    (void)events;
    if (gpio != PIN_CLK) return;

    // Drop any edge that comes too soon after the previous one — contact bounce.
    uint32_t now = time_us_32();
    if ((now - s_last_edge_us) < DEBOUNCE_US) return;

    int clk = gpio_get(PIN_CLK);
    if (clk == s_last_clk) return;
    s_last_clk     = clk;
    s_last_edge_us = now;

    // Count one event per detent on the falling edge.
    if (clk == 0) {
        if (gpio_get(PIN_DT) == 1) s_pending_delta += 1;
        else                       s_pending_delta -= 1;
    }
}

void encoder_init(encoder_rotate_fn on_rotate,
                  encoder_press_fn  on_short_press,
                  encoder_press_fn  on_long_press) {
    s_on_rotate = on_rotate;
    s_on_short  = on_short_press;
    s_on_long   = on_long_press;

    gpio_init(PIN_CLK); gpio_set_dir(PIN_CLK, GPIO_IN); gpio_pull_up(PIN_CLK);
    gpio_init(PIN_DT);  gpio_set_dir(PIN_DT,  GPIO_IN); gpio_pull_up(PIN_DT);
    gpio_init(PIN_SW);  gpio_set_dir(PIN_SW,  GPIO_IN); gpio_pull_up(PIN_SW);

    s_last_clk = gpio_get(PIN_CLK);

    gpio_set_irq_enabled_with_callback(
        PIN_CLK,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &clk_irq);
}

void encoder_set_double_press(encoder_press_fn on_double_press) {
    s_on_double = on_double_press;
}

void encoder_tick(void) {
    // Drain pending rotation safely
    uint32_t ints = save_and_disable_interrupts();
    int delta = s_pending_delta;
    s_pending_delta = 0;
    restore_interrupts(ints);

    if (delta != 0 && s_on_rotate) s_on_rotate(delta);

    int sw = gpio_get(PIN_SW);
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (s_sw_last == 1 && sw == 0) {
        // Press edge.
        if (s_short_pending && s_on_double) {
            // Second press inside the double-press window → fire double.
            s_short_pending = false;
            s_on_double();
            // Suppress short/long for this press: mark as already-fired.
            s_long_fired = true;
        }
        s_press_ms = now;
        if (!s_long_fired) s_long_fired = false;
    } else if (s_sw_last == 0 && sw == 1) {
        // Release edge.
        if (!s_long_fired) {
            uint32_t held = now - s_press_ms;
            if (held < LONG_PRESS_MS) {
                if (s_on_double) {
                    // Defer the short press to allow a second click.
                    s_short_pending    = true;
                    s_short_pending_ms = now;
                } else if (s_on_short) {
                    s_on_short();
                }
            }
        }
        s_long_fired = false;
    } else if (s_sw_last == 0 && sw == 0) {
        if (!s_long_fired) {
            uint32_t held = now - s_press_ms;
            if (held >= LONG_PRESS_MS) {
                s_long_fired = true;
                s_short_pending = false;
                if (s_on_long) s_on_long();
            }
        }
    }
    s_sw_last = sw;

    // Flush deferred short press once the double-press window expires.
    if (s_short_pending && (now - s_short_pending_ms) >= DOUBLE_PRESS_MS) {
        s_short_pending = false;
        if (s_on_short) s_on_short();
    }
}
