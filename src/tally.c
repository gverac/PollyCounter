#include "tally.h"

#include <stdio.h>

#include "state.h"
#include "display.h"
#include "gfx.h"

static int s_active = 0;   // 0..TALLY_SLOTS-1

static void draw(void);

static void on_enter(void) {
    if (s_active < 0 || s_active >= TALLY_SLOTS) s_active = 0;
    draw();
}

static void on_rotate(int delta) {
    int v = s_active + delta;
    if (v < 0) v = 0;
    if (v >= TALLY_SLOTS) v = TALLY_SLOTS - 1;
    s_active = v;
    draw();
}

static void on_short_press(void) {
    state_tally_reset(s_active);
    draw();
}

static void on_increment(void) {
    state_tally_increment(s_active);
    draw();
}

static void on_decrement(void) {
    state_tally_decrement(s_active);
    draw();
}

// ── Drawing ────────────────────────────────────────────────────────────────

static void draw_centered(const char *s, int y, int scale, uint16_t color) {
    int w = display_text_width(s, scale);
    display_text(s, (GFX_W - w) / 2, y, scale, color);
}

static void draw_cell(int slot, int x, int y, int w, int h, bool active) {
    uint16_t border = active ? C_ACCENT : C_DIVIDER;
    gfx_rect(x, y, w, h, border);
    if (active) gfx_rect(x + 1, y + 1, w - 2, h - 2, border);

    char letter[2] = { (char)('A' + slot), 0 };
    display_text(letter, x + 4, y + 4, 1, active ? C_ACCENT : C_LABEL);

    char buf[8];
    uint32_t v = state_tally_count(slot);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(v > 9999 ? 9999 : v));

    int scale = 3;
    int tw = display_text_width(buf, scale);
    while (tw > w - 6 && scale > 1) { scale--; tw = display_text_width(buf, scale); }
    int th = 8 * scale;
    int tx = x + (w - tw) / 2;
    int ty = y + (h - th) / 2 + 2;
    display_text(buf, tx, ty, scale, active ? C_WHITE : C_INK_DIM);
}

static void draw(void) {
    gfx_clear(C_BLACK);
    draw_centered("MULTI-TALLY", 4, 1, C_LABEL);

    // 2 columns × 2 rows
    const int margin = 4;
    const int top    = 16;
    const int gap    = 4;
    int cell_w = (GFX_W - 2 * margin - gap) / 2;
    int cell_h = 50;

    for (int i = 0; i < TALLY_SLOTS; i++) {
        int row = i / 2;
        int col = i % 2;
        int x = margin + col * (cell_w + gap);
        int y = top + row * (cell_h + gap);
        draw_cell(i, x, y, cell_w, cell_h, i == s_active);
    }

    draw_centered("ROTATE = PICK",  130, 1, C_LABEL);
    draw_centered("o RESET  +/- ADJ", 146, 1, C_LABEL);
    gfx_present();
}

const program_t TALLY_PROGRAM = {
    .name           = "Multi-tally",
    .on_enter       = on_enter,
    .on_rotate      = on_rotate,
    .on_short_press = on_short_press,
    .on_increment   = on_increment,
    .on_decrement   = on_decrement,
    .draw           = draw,
};
