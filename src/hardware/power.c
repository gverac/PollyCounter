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

#define DEFAULT_DIM_AFTER_MS         30000u
#define DEFAULT_AUTO_SLEEP_AFTER_MS  (10u * 60u * 1000u)
#define PWM_WRAP   65535
#define DIM_FRACTION_PCT 20   // dim level is 20% of full brightness

static uint     s_slice;
static uint     s_chan;
static uint32_t s_last_activity_ms = 0;
static bool     s_is_dim = false;

static uint32_t s_dim_after_ms   = DEFAULT_DIM_AFTER_MS;
static uint32_t s_sleep_after_ms = DEFAULT_AUTO_SLEEP_AFTER_MS;
static uint8_t  s_brightness_pct = 100;

// Captured once at boot so we can restore after dormant wake.
static uint32_t s_scb_orig    = 0;
static uint32_t s_clock0_orig = 0;
static uint32_t s_clock1_orig = 0;

static inline uint16_t pwm_level_from_pct(uint8_t pct) {
    if (pct > 100) pct = 100;
    return (uint16_t)((uint32_t)PWM_WRAP * pct / 100u);
}

void power_init(void) {
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
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(s_slice, &cfg, true);

    power_set_full();
    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
}

void power_configure(uint32_t auto_dim_ms, uint32_t auto_sleep_ms, uint8_t brightness_pct) {
    if (auto_dim_ms)   s_dim_after_ms   = auto_dim_ms;
    if (auto_sleep_ms) s_sleep_after_ms = auto_sleep_ms;
    if (brightness_pct < 5)   brightness_pct = 5;
    if (brightness_pct > 100) brightness_pct = 100;
    s_brightness_pct = brightness_pct;
    if (!s_is_dim) power_set_full();
}

void power_set_brightness(uint8_t pct) {
    if (pct < 5)   pct = 5;
    if (pct > 100) pct = 100;
    s_brightness_pct = pct;
    if (!s_is_dim) power_set_full();
}

void power_set_full(void) {
    pwm_set_chan_level(s_slice, s_chan, pwm_level_from_pct(s_brightness_pct));
    s_is_dim = false;
}

void power_set_dim(void) {
    uint8_t dim_pct = (uint8_t)((uint32_t)s_brightness_pct * DIM_FRACTION_PCT / 100u);
    if (dim_pct < 2) dim_pct = 2;
    pwm_set_chan_level(s_slice, s_chan, pwm_level_from_pct(dim_pct));
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
    if ((now - s_last_activity_ms) >= s_dim_after_ms) {
        power_set_dim();
    }
}

bool power_should_auto_sleep(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    return (now - s_last_activity_ms) >= s_sleep_after_ms;
}

static void recover_from_sleep(void) {
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);
    scb_hw->scr           = s_scb_orig;
    clocks_hw->sleep_en0  = s_clock0_orig;
    clocks_hw->sleep_en1  = s_clock1_orig;
    clocks_init();
}

void power_go_dormant(void) {
    power_set_off();
    sleep_ms(100);

    sleep_run_from_xosc();
    sleep_goto_dormant_until_pin(PIN_WAKE, /*edge=*/true, /*high=*/false);

    recover_from_sleep();

    s_last_activity_ms = to_ms_since_boot(get_absolute_time());
}
