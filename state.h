#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <stdbool.h>

void state_load(void);
void state_save(void);   // immediate write to flash
void state_tick(void);   // call from main loop; flushes if dirty + 5s elapsed

uint32_t state_get_count(void);
uint32_t state_get_target(void);

void state_increment(void);
void state_decrement(void);
void state_reset_count(void);
void state_set_target(uint32_t value);

bool state_is_dirty(void);

#endif
