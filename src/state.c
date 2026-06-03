#include "state.h"

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Reserve the final sector of the 2 MB flash for our state.
// Pico W has 2 MB flash; classic Pico has 2 MB as well. We store at the very
// end so we never collide with program code.
#define STATE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#define STATE_MAGIC 0x53544348u  // "STCH"
#define SAVE_DELAY_MS 5000

typedef struct {
    uint32_t magic;
    uint32_t count;
    uint32_t target;
    uint32_t crc;
} state_record_t;

static uint32_t s_count  = 0;
static uint32_t s_target = 0;

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

void state_load(void) {
    const state_record_t *rec =
        (const state_record_t *)(XIP_BASE + STATE_FLASH_OFFSET);

    if (rec->magic == STATE_MAGIC) {
        uint32_t expected = crc32(rec, sizeof(*rec) - sizeof(uint32_t));
        if (expected == rec->crc) {
            s_count  = rec->count;
            s_target = rec->target;
            s_dirty  = false;
            s_last_change_ms = 0;
            return;
        }
    }
    // Fresh / corrupt — start at zero
    s_count = 0;
    s_target = 0;
    s_dirty = false;
    s_last_change_ms = 0;
}

void state_save(void) {
    state_record_t rec = {
        .magic  = STATE_MAGIC,
        .count  = s_count,
        .target = s_target,
        .crc    = 0,
    };
    rec.crc = crc32(&rec, sizeof(rec) - sizeof(uint32_t));

    // Flash writes must be a multiple of FLASH_PAGE_SIZE (256B).
    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));
    memcpy(page, &rec, sizeof(rec));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(STATE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(STATE_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    s_dirty = false;
    s_last_change_ms = 0;
}

static void mark_changed(void) {
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

uint32_t state_get_count(void)  { return s_count; }
uint32_t state_get_target(void) { return s_target; }

void state_increment(void) {
    if (s_count >= 999) return;
    s_count++;
    mark_changed();
}

void state_decrement(void) {
    if (s_count > 0) {
        s_count--;
        mark_changed();
    }
}

void state_reset_count(void) {
    s_count = 0;
    mark_changed();
}

void state_set_target(uint32_t value) {
    if (value > 999) value = 999;
    s_target = value;
    mark_changed();
}

bool state_is_dirty(void) { return s_dirty; }
