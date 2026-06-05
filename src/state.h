#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PROGRAM_COUNTER   = 0,
    PROGRAM_REP       = 1,
    PROGRAM_COUNTDOWN = 2,
    PROGRAM_STOPWATCH = 3,
    PROGRAM_SETTINGS  = 4,
    PROGRAM_INTERVAL  = 5,
    PROGRAM_TALLY     = 6,
    PROGRAM__COUNT
} program_id_t;

#define TALLY_SLOTS 4

void state_load(void);
void state_save(void);   // immediate write to flash
void state_tick(void);   // call from main loop; flushes if dirty + 5s elapsed

void state_mark_dirty(void);
bool state_is_dirty(void);

// Which program to land in on boot/wake.
program_id_t state_get_current_program(void);
void         state_set_current_program(program_id_t id);

// ── Counter ────────────────────────────────────────────────────────────────
uint32_t state_get_count(void);
uint32_t state_get_target(void);
void     state_increment(void);
void     state_decrement(void);
void     state_reset_count(void);
void     state_set_target(uint32_t value);

// ── Rep counter ────────────────────────────────────────────────────────────
uint32_t state_rep_get_target_reps(void);
uint32_t state_rep_get_target_sets(void);
void     state_rep_set_target_reps(uint32_t v);
void     state_rep_set_target_sets(uint32_t v);

// ── Countdown ──────────────────────────────────────────────────────────────
uint32_t state_countdown_get_seconds(void);
void     state_countdown_set_seconds(uint32_t v);

// ── Interval timer ─────────────────────────────────────────────────────────
uint32_t state_interval_work_s(void);
uint32_t state_interval_rest_s(void);
uint32_t state_interval_rounds(void);
void     state_interval_set_work_s(uint32_t v);
void     state_interval_set_rest_s(uint32_t v);
void     state_interval_set_rounds(uint32_t v);

// ── Multi-tally ────────────────────────────────────────────────────────────
uint32_t state_tally_count(int slot);                  // 0..TALLY_SLOTS-1
void     state_tally_set_count(int slot, uint32_t v);
void     state_tally_increment(int slot);
void     state_tally_decrement(int slot);
void     state_tally_reset(int slot);

// ── Settings ───────────────────────────────────────────────────────────────
uint8_t  state_settings_brightness(void);          // 0..100 (%)
uint32_t state_settings_auto_sleep_ms(void);
uint32_t state_settings_auto_dim_ms(void);
void     state_settings_set_brightness(uint8_t pct);
void     state_settings_set_auto_sleep_ms(uint32_t ms);
void     state_settings_set_auto_dim_ms(uint32_t ms);

// Wipe all persisted state back to defaults (does not reboot).
void state_reset_all(void);

#endif
