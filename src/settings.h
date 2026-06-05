#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "program.h"

extern const program_t SETTINGS_PROGRAM;

// Apply persisted settings to power module.
void settings_apply_to_power(void);

#endif
