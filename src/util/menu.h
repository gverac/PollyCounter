#ifndef MENU_H_
#define MENU_H_

#include "program.h"

extern const program_t MENU_PROGRAM;

// Reset cursor to the top (Sleep). Called by main when opening the menu so
// "long press, press" always sleeps regardless of where the cursor was left.
void menu_reset_cursor(void);

#endif
