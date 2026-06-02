#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

typedef void (*encoder_rotate_fn)(int delta);
typedef void (*encoder_press_fn)(void);

void encoder_init(encoder_rotate_fn on_rotate,
                  encoder_press_fn  on_short_press,
                  encoder_press_fn  on_long_press);

void encoder_tick(void);

#endif
