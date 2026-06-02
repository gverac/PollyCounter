#ifndef BUTTONS_H_
#define BUTTONS_H_

#include <stdint.h>

typedef void (*button_press_fn)(void);

void buttons_init(button_press_fn on_increment, button_press_fn on_decrement);
void buttons_tick(void);

// Optional: register a callback that fires once both buttons have been held
// simultaneously for `threshold_ms`. Individual press events are suppressed
// whenever both buttons are detected pressed (so a quick double-tap won't
// register as inc + dec).
void buttons_set_combo_handler(button_press_fn on_combo, uint32_t threshold_ms);

#endif
