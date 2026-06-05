#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

typedef void (*encoder_rotate_fn)(int delta);
typedef void (*encoder_press_fn)(void);

void encoder_init(encoder_rotate_fn on_rotate,
                  encoder_press_fn  on_short_press,
                  encoder_press_fn  on_long_press);

// Optional: register a double-press callback. When set, single short presses
// are deferred by the double-press window (~350 ms); a second press inside the
// window fires on_double_press instead of a second on_short_press.
void encoder_set_double_press(encoder_press_fn on_double_press);

void encoder_tick(void);

#endif
