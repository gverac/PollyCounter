#include "power.h"

#include "pico/stdlib.h"
#include "pico/sleep.h"
#include "pico/runtime_init.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"

#define PIN_BL    22
#define PIN_WAKE  15

#define DIM_AFTER_MS         30000               // 30s → dim backlight
#define AUTO_SLEEP_AFTER_MS  (10u * 60u * 1000u) // 10min → dormant
#define FULL_LEVEL   65535
#define DIM_LEVEL    13107   // ~20%

static uint     s_slice;
static uint     s_chan;
static uint32_t s_last_activity_ms = 0;
static bool     s_is_dim = false;

// Captured once at boot so we can restore after dormant wake.
static uint32_t s_scb_orig    = 0;
static uint32_t s_clock0_orig = 0;
static uint32_t s_clock1_orig = 0;

void power_init(void) {
    // Capture clock state on first init only (skip on post-wake re-init).
    static bool captured = false;
    if (!captured) {
        s_scb_orig    = scb_hw->scr;
        s_clock0_orig = clocks_hw->sleep_en0;
        s_clock1_orig = clocks_hw->sleep_en1;
        captured = true;
    }

    gpio_set_function(PIN_BL, GPIO_FUNC_PWM);
    s_slice = pwm_gpio_to_slice_num(PIN_BL);
    s_chan  = pwm_gpio_to_channel(PIN_BL);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, FULL_LEVEL);
    pwm_init(s_slice, &cfg, true);

    power_set_full();
    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
}

void power_set_full(void) {
    pwm_set_chan_level(s_slice, s_chan, FULL_LEVEL);
    s_is_dim = false;
}

void power_set_dim(void) {
    pwm_set_chan_level(s_slice, s_chan, DIM_LEVEL);
    s_is_dim = true;
}

void power_set_off(void) {
    pwm_set_chan_level(s_slice, s_chan, 0);
}

bool power_is_dim(void) { return s_is_dim; }

void power_notify_activity(void) {
    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
    if (s_is_dim) power_set_full();
}

void power_tick(void) {
    if (s_is_dim) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((now - s_last_activity_ms) >= DIM_AFTER_MS) {
        power_set_dim();
    }
}

bool power_should_auto_sleep(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    return (now - s_last_activity_ms) >= AUTO_SLEEP_AFTER_MS;
}

static void recover_from_sleep(void) {
    // Re-enable the ring oscillator — clocks_init() needs it.
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    // Restore processor + clock-block sleep enables to boot-time values.
    scb_hw->scr           = s_scb_orig;
    clocks_hw->sleep_en0  = s_clock0_orig;
    clocks_hw->sleep_en1  = s_clock1_orig;

    // Bring the clock tree back up.
    clocks_init();
}

void power_go_dormant(void) {
    power_set_off();
    sleep_ms(100);

    // Switch system clocks to XOSC so we can actually go dormant.
    sleep_run_from_xosc();

    // Sleep until GP15 goes low (encoder button press, active-low).
    sleep_goto_dormant_until_pin(PIN_WAKE, /*edge=*/true, /*high=*/false);

    // ── Woken ──
    recover_from_sleep();

    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
}
