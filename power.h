#ifndef POWER_H
#define POWER_H

#include <stdbool.h>

void power_init(void);
void power_set_full(void);
void power_set_dim(void);
void power_set_off(void);
bool power_is_dim(void);

void power_notify_activity(void);
void power_tick(void);

// True when more than the auto-sleep threshold has elapsed since last activity.
bool power_should_auto_sleep(void);

// Save backlight off, go to dormant; resumes on GP15 falling edge.
// Returns after wake — caller should re-init display.
void power_go_dormant(void);

#endif
