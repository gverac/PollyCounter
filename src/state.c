#include "state.h"

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#define STATE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#define STATE_MAGIC_V1 0x53544348u  // "STCH" — legacy single-counter record
#define STATE_MAGIC_V2 0x504F4C32u  // "POL2" — multi-program record
#define SAVE_DELAY_MS  5000

#define DEFAULT_AUTO_DIM_MS    (30u  * 1000u)
#define DEFAULT_AUTO_SLEEP_MS  (10u  * 60u * 1000u)
#define DEFAULT_BRIGHTNESS_PCT 100

typedef struct {
    uint32_t magic;        // STATE_MAGIC_V1
    uint32_t count;
    uint32_t target;
    uint32_t crc;
} state_record_v1_t;

typedef struct {
    uint32_t magic;        // STATE_MAGIC_V2
    uint32_t version;      // 1
    uint32_t current_program;

    uint32_t count;
    uint32_t target;

    uint32_t rep_target_reps;
    uint32_t rep_target_sets;

    uint32_t countdown_seconds;

    uint32_t brightness_pct;
    uint32_t auto_sleep_ms;
    uint32_t auto_dim_ms;

    uint32_t interval_work_s;
    uint32_t interval_rest_s;
    uint32_t interval_rounds;

    uint32_t tally_count[TALLY_SLOTS];

    uint32_t reserved[1];

    uint32_t crc;
} state_record_v2_t;

static state_record_v2_t s = {0};

static bool     s_dirty = false;
static uint32_t s_last_change_ms = 0;

static uint32_t crc32(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(int32_t)(crc & 1));
        }
    }
    return ~crc;
}

static void set_defaults(void) {
    memset(&s, 0, sizeof(s));
    s.magic           = STATE_MAGIC_V2;
    s.version         = 1;
    s.current_program = PROGRAM_COUNTER;
    s.rep_target_reps   = 10;
    s.rep_target_sets   = 3;
    s.countdown_seconds = 30;
    s.brightness_pct  = DEFAULT_BRIGHTNESS_PCT;
    s.auto_sleep_ms   = DEFAULT_AUTO_SLEEP_MS;
    s.auto_dim_ms     = DEFAULT_AUTO_DIM_MS;
    s.interval_work_s = 30;
    s.interval_rest_s = 10;
    s.interval_rounds = 8;
}

void state_load(void) {
    set_defaults();

    const uint32_t *magic = (const uint32_t *)(XIP_BASE + STATE_FLASH_OFFSET);

    if (*magic == STATE_MAGIC_V2) {
        const state_record_v2_t *rec =
            (const state_record_v2_t *)(XIP_BASE + STATE_FLASH_OFFSET);
        uint32_t expected = crc32(rec, sizeof(*rec) - sizeof(uint32_t));
        if (expected == rec->crc) {
            s = *rec;
        }
    } else if (*magic == STATE_MAGIC_V1) {
        const state_record_v1_t *rec =
            (const state_record_v1_t *)(XIP_BASE + STATE_FLASH_OFFSET);
        uint32_t expected = crc32(rec, sizeof(*rec) - sizeof(uint32_t));
        if (expected == rec->crc) {
            s.count  = rec->count;
            s.target = rec->target;
        }
    }

    s_dirty = false;
    s_last_change_ms = 0;
}

void state_save(void) {
    s.magic   = STATE_MAGIC_V2;
    s.version = 1;
    s.crc = crc32(&s, sizeof(s) - sizeof(uint32_t));

    // Pad to a whole flash page (256B); record is well under that.
    _Static_assert(sizeof(state_record_v2_t) <= FLASH_PAGE_SIZE, "state too big");
    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &s, sizeof(s));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(STATE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(STATE_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    s_dirty = false;
    s_last_change_ms = 0;
}

void state_mark_dirty(void) {
    s_dirty = true;
    s_last_change_ms = to_ms_since_boot(get_absolute_time());
}

void state_tick(void) {
    if (!s_dirty) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((now - s_last_change_ms) >= SAVE_DELAY_MS) {
        state_save();
    }
}

bool state_is_dirty(void) { return s_dirty; }

program_id_t state_get_current_program(void) {
    if (s.current_program >= PROGRAM__COUNT) return PROGRAM_COUNTER;
    return (program_id_t)s.current_program;
}
void state_set_current_program(program_id_t id) {
    if ((uint32_t)id == s.current_program) return;
    s.current_program = (uint32_t)id;
    state_mark_dirty();
}

// ── Counter ────────────────────────────────────────────────────────────────
uint32_t state_get_count(void)  { return s.count; }
uint32_t state_get_target(void) { return s.target; }

void state_increment(void) {
    if (s.count >= 999) return;
    s.count++;
    state_mark_dirty();
}
void state_decrement(void) {
    if (s.count > 0) { s.count--; state_mark_dirty(); }
}
void state_reset_count(void) {
    s.count = 0;
    state_mark_dirty();
}
void state_set_target(uint32_t value) {
    if (value > 999) value = 999;
    s.target = value;
    state_mark_dirty();
}

// ── Rep counter ────────────────────────────────────────────────────────────
uint32_t state_rep_get_target_reps(void) { return s.rep_target_reps; }
uint32_t state_rep_get_target_sets(void) { return s.rep_target_sets; }
void state_rep_set_target_reps(uint32_t v) {
    if (v > 999) v = 999;
    if (v < 1)   v = 1;
    s.rep_target_reps = v;
    state_mark_dirty();
}
void state_rep_set_target_sets(uint32_t v) {
    if (v > 999) v = 999;
    if (v < 1)   v = 1;
    s.rep_target_sets = v;
    state_mark_dirty();
}

// ── Countdown ──────────────────────────────────────────────────────────────
uint32_t state_countdown_get_seconds(void) { return s.countdown_seconds; }
void state_countdown_set_seconds(uint32_t v) {
    if (v > 5999) v = 5999;   // 99:59 max
    s.countdown_seconds = v;
    state_mark_dirty();
}

// ── Settings ───────────────────────────────────────────────────────────────
uint8_t  state_settings_brightness(void) {
    uint32_t v = s.brightness_pct;
    if (v > 100) v = 100;
    if (v < 5)   v = 5;
    return (uint8_t)v;
}
uint32_t state_settings_auto_sleep_ms(void) {
    return s.auto_sleep_ms ? s.auto_sleep_ms : DEFAULT_AUTO_SLEEP_MS;
}
uint32_t state_settings_auto_dim_ms(void) {
    return s.auto_dim_ms ? s.auto_dim_ms : DEFAULT_AUTO_DIM_MS;
}
void state_settings_set_brightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    if (pct < 5)   pct = 5;
    s.brightness_pct = pct;
    state_mark_dirty();
}
void state_settings_set_auto_sleep_ms(uint32_t ms) {
    s.auto_sleep_ms = ms;
    state_mark_dirty();
}
void state_settings_set_auto_dim_ms(uint32_t ms) {
    s.auto_dim_ms = ms;
    state_mark_dirty();
}

// ── Interval timer ─────────────────────────────────────────────────────────
uint32_t state_interval_work_s(void) { return s.interval_work_s; }
uint32_t state_interval_rest_s(void) { return s.interval_rest_s; }
uint32_t state_interval_rounds(void) { return s.interval_rounds; }

void state_interval_set_work_s(uint32_t v) {
    if (v < 1)    v = 1;
    if (v > 5999) v = 5999;
    s.interval_work_s = v; state_mark_dirty();
}
void state_interval_set_rest_s(uint32_t v) {
    if (v > 5999) v = 5999;
    s.interval_rest_s = v; state_mark_dirty();
}
void state_interval_set_rounds(uint32_t v) {
    if (v < 1)  v = 1;
    if (v > 99) v = 99;
    s.interval_rounds = v; state_mark_dirty();
}

// ── Multi-tally ────────────────────────────────────────────────────────────
uint32_t state_tally_count(int slot) {
    if (slot < 0 || slot >= TALLY_SLOTS) return 0;
    return s.tally_count[slot];
}
void state_tally_set_count(int slot, uint32_t v) {
    if (slot < 0 || slot >= TALLY_SLOTS) return;
    if (v > 9999) v = 9999;
    s.tally_count[slot] = v; state_mark_dirty();
}
void state_tally_increment(int slot) {
    if (slot < 0 || slot >= TALLY_SLOTS) return;
    if (s.tally_count[slot] >= 9999) return;
    s.tally_count[slot]++; state_mark_dirty();
}
void state_tally_decrement(int slot) {
    if (slot < 0 || slot >= TALLY_SLOTS) return;
    if (s.tally_count[slot] == 0) return;
    s.tally_count[slot]--; state_mark_dirty();
}
void state_tally_reset(int slot) {
    if (slot < 0 || slot >= TALLY_SLOTS) return;
    s.tally_count[slot] = 0; state_mark_dirty();
}

void state_reset_all(void) {
    set_defaults();
    state_save();
}
