#include "counter.h"

#include "state.h"
#include "display.h"

static void on_enter(void) {
    display_draw_screen(state_get_count(), state_get_target());
}

static void on_rotate(int delta) {
    int32_t cur  = (int32_t)state_get_target();
    int32_t next = cur + delta;
    if (next < 0) next = 0;
    state_set_target((uint32_t)next);
    display_update_target(state_get_count(), state_get_target());
}

static void on_short_press(void) {
    state_reset_count();
    display_update_count(0, state_get_target());
}

static void on_increment(void) {
    state_increment();
    display_update_count(state_get_count(), state_get_target());
}

static void on_decrement(void) {
    state_decrement();
    display_update_count(state_get_count(), state_get_target());
}

static void tick(void) {
    display_tick();
}

const program_t COUNTER_PROGRAM = {
    .name         = "Counter",
    .on_enter     = on_enter,
    .on_rotate    = on_rotate,
    .on_short_press = on_short_press,
    .on_increment = on_increment,
    .on_decrement = on_decrement,
    .tick         = tick,
    .draw         = on_enter,
};
