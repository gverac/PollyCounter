#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include <stdint.h>

void power_init(void);
void power_set_full(void);
void power_set_dim(void);
void power_set_off(void);
bool power_is_dim(void);

// Configurable timeouts (ms) and brightness (0..100 %).
void power_configure(uint32_t auto_dim_ms, uint32_t auto_sleep_ms, uint8_t brightness_pct);
void power_set_brightness(uint8_t pct);   // 5..100

void power_notify_activity(void);
void power_tick(void);

// True when more than the auto-sleep threshold has elapsed since last activity.
bool power_should_auto_sleep(void);

// Save backlight off, go to dormant; resumes on GP15 falling edge.
// Returns after wake — caller should re-init display.
void power_go_dormant(void);

#endif
