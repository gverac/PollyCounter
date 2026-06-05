#include "rep.h"

#include <stdio.h>

#include "state.h"
#include "display.h"
#include "gfx.h"

typedef enum {
    PHASE_SET_REPS,
    PHASE_SET_SETS,
    PHASE_LIVE,
} phase_t;

static phase_t  s_phase;
static uint32_t s_reps;        // current reps in active set
static uint32_t s_sets;        // completed sets

// Editable buffers used during setup so the saved value only commits on confirm.
static uint32_t s_edit_reps;
static uint32_t s_edit_sets;

static void draw(void);

static void on_enter(void) {
    s_reps = 0;
    s_sets = 0;
    s_edit_reps = state_rep_get_target_reps();
    s_edit_sets = state_rep_get_target_sets();
    s_phase = PHASE_SET_REPS;
    draw();
}

static void on_rotate(int delta) {
    if (s_phase == PHASE_SET_REPS) {
        int32_t v = (int32_t)s_edit_reps + delta;
        if (v < 1)   v = 1;
        if (v > 999) v = 999;
        s_edit_reps = (uint32_t)v;
        draw();
    } else if (s_phase == PHASE_SET_SETS) {
        int32_t v = (int32_t)s_edit_sets + delta;
        if (v < 1)   v = 1;
        if (v > 999) v = 999;
        s_edit_sets = (uint32_t)v;
        draw();
    }
}

static void on_short_press(void) {
    if (s_phase == PHASE_SET_REPS) {
        state_rep_set_target_reps(s_edit_reps);
        s_phase = PHASE_SET_SETS;
    } else if (s_phase == PHASE_SET_SETS) {
        state_rep_set_target_sets(s_edit_sets);
        s_phase = PHASE_LIVE;
        s_reps = 0;
        s_sets = 0;
    } else {
        // Live: reset counts.
        s_reps = 0;
        s_sets = 0;
    }
    draw();
}

static void on_increment(void) {
    if (s_phase != PHASE_LIVE) return;
    s_reps++;
    if (s_reps >= state_rep_get_target_reps()) {
        s_reps = 0;
        if (s_sets < state_rep_get_target_sets()) s_sets++;
    }
    draw();
}

static void on_decrement(void) {
    if (s_phase != PHASE_LIVE) return;
    if (s_reps > 0) {
        s_reps--;
    } else if (s_sets > 0) {
        s_sets--;
        s_reps = state_rep_get_target_reps() - 1;
    }
    draw();
}

// ── Drawing ────────────────────────────────────────────────────────────────

static void draw_centered(const char *s, int y, int scale, uint16_t color) {
    int w = display_text_width(s, scale);
    display_text(s, (GFX_W - w) / 2, y, scale, color);
}

static void draw_setup(const char *prompt, uint32_t value) {
    gfx_clear(C_BLACK);
    draw_centered("REP MODE", 6, 1, C_LABEL);
    draw_centered(prompt, 22, 2, C_WHITE);

    char buf[8];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    int scale = 5;
    int w = display_text_width(buf, scale);
    while (w > GFX_W - 8 && scale > 2) { scale--; w = display_text_width(buf, scale); }
    display_text(buf, (GFX_W - w) / 2, 60, scale, C_ACCENT);

    draw_centered("ROTATE TO SET",    120, 1, C_LABEL);
    draw_centered("PRESS TO CONFIRM", 134, 1, C_LABEL);
    gfx_present();
}

static void draw_live(void) {
    gfx_clear(C_BLACK);
    draw_centered("REPS",  4, 2, C_LABEL);

    char buf[16];
    snprintf(buf, sizeof(buf), "%lu/%lu",
             (unsigned long)s_reps,
             (unsigned long)state_rep_get_target_reps());
    int scale = 4;
    int w = display_text_width(buf, scale);
    while (w > GFX_W - 4 && scale > 2) { scale--; w = display_text_width(buf, scale); }
    display_text(buf, (GFX_W - w) / 2, 26, scale, C_WHITE);

    draw_centered("SETS", 78, 2, C_LABEL);
    snprintf(buf, sizeof(buf), "%lu/%lu",
             (unsigned long)s_sets,
             (unsigned long)state_rep_get_target_sets());
    scale = 4;
    w = display_text_width(buf, scale);
    while (w > GFX_W - 4 && scale > 2) { scale--; w = display_text_width(buf, scale); }
    display_text(buf, (GFX_W - w) / 2, 100, scale, C_ACCENT);

    draw_centered("PRESS=RESET", 148, 1, C_LABEL);
    gfx_present();
}

static void draw(void) {
    switch (s_phase) {
        case PHASE_SET_REPS: draw_setup("SET REPS",  s_edit_reps); break;
        case PHASE_SET_SETS: draw_setup("SET SETS",  s_edit_sets); break;
        case PHASE_LIVE:     draw_live(); break;
    }
}

const program_t REP_PROGRAM = {
    .name           = "Rep Counter",
    .on_enter       = on_enter,
    .on_rotate      = on_rotate,
    .on_short_press = on_short_press,
    .on_increment   = on_increment,
    .on_decrement   = on_decrement,
    .draw           = draw,
};
