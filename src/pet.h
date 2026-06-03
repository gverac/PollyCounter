// Pet mode — sheep tamagotchi easter egg.
// Enter with both buttons held 5s. Exits after 5s of no interaction.

#ifndef PET_H_
#define PET_H_

#include <stdbool.h>

void pet_enter(void);
void pet_tick(void);
bool pet_is_active(void);

// Inputs (called from main when in pet mode)
void pet_on_scratch(void);    // INC button — pet the sheep
void pet_on_feed(void);       // DEC button — feed the sheep
void pet_on_rotate(int d);    // encoder rotate — sheep walks
void pet_on_trick(void);      // encoder short press — sheep spins

#endif
