#include "settings.h"

#include <stdio.h>

#include "state.h"
#include "display.h"
#include "gfx.h"
#include "power.h"

typedef enum {
    ROW_BRIGHTNESS,
    ROW_AUTO_SLEEP,
    ROW_AUTO_DIM,
    ROW_RESET,
    ROW__COUNT
} row_t;

static int  s_cursor;
static bool s_editing;
static bool s_confirm_reset;

static void draw(void);

void settings_apply_to_power(void) {
    power_configure(state_settings_auto_dim_ms(),
                    state_settings_auto_sleep_ms(),
                    state_settings_brightness());
}

static void on_enter(void) {
    s_cursor = 0;
    s_editing = false;
    s_confirm_reset = false;
    draw();
}

static void edit_brightness(int delta) {
    int v = (int)state_settings_brightness() + delta * 5;
    if (v < 5)   v = 5;
    if (v > 100) v = 100;
    state_settings_set_brightness((uint8_t)v);
    power_set_brightness((uint8_t)v);
}

static void edit_auto_sleep(int delta) {
    // step 30s under 5min, 1min between 5-30min, 5min above
    uint32_t cur = state_settings_auto_sleep_ms() / 1000;
    int step;
    if (cur < 300)      step = 30;
    else if (cur < 1800) step = 60;
    else                 step = 300;
    int32_t v = (int32_t)cur + delta * step;
    if (v < 30)    v = 30;
    if (v > 3600)  v = 3600;
    state_settings_set_auto_sleep_ms((uint32_t)v * 1000u);
    settings_apply_to_power();
}

static void edit_auto_dim(int delta) {
    uint32_t cur = state_settings_auto_dim_ms() / 1000;
    int step = (cur < 60) ? 5 : 15;
    int32_t v = (int32_t)cur + delta * step;
    if (v < 5)    v = 5;
    if (v > 600)  v = 600;
    state_settings_set_auto_dim_ms((uint32_t)v * 1000u);
    settings_apply_to_power();
}

static void on_rotate(int delta) {
    if (s_confirm_reset) return;
    if (s_editing) {
        switch (s_cursor) {
            case ROW_BRIGHTNESS: edit_brightness(delta); break;
            case ROW_AUTO_SLEEP: edit_auto_sleep(delta); break;
            case ROW_AUTO_DIM:   edit_auto_dim(delta);   break;
            default: break;
        }
    } else {
        int v = s_cursor + delta;
        if (v < 0) v = 0;
        if (v >= ROW__COUNT) v = ROW__COUNT - 1;
        s_cursor = v;
    }
    draw();
}

static void on_short_press(void) {
    if (s_confirm_reset) {
        state_reset_all();
        settings_apply_to_power();
        s_confirm_reset = false;
        draw();
        return;
    }
    if (s_cursor == ROW_RESET) {
        s_confirm_reset = true;
    } else {
        s_editing = !s_editing;
    }
    draw();
}

static void on_decrement(void) {
    // Cancel reset confirmation or editing.
    if (s_confirm_reset) { s_confirm_reset = false; draw(); return; }
    if (s_editing)       { s_editing = false; draw(); return; }
}

// ── Drawing ────────────────────────────────────────────────────────────────

static void fmt_seconds(uint32_t ms, char *buf, size_t n) {
    uint32_t s = ms / 1000;
    if (s < 60) snprintf(buf, n, "%lus", (unsigned long)s);
    else        snprintf(buf, n, "%lum%02lus",
                         (unsigned long)(s / 60),
                         (unsigned long)(s % 60));
}

static void row_value(row_t r, char *buf, size_t n) {
    switch (r) {
        case ROW_BRIGHTNESS:
            snprintf(buf, n, "%u%%", state_settings_brightness());
            break;
        case ROW_AUTO_SLEEP:
            fmt_seconds(state_settings_auto_sleep_ms(), buf, n);
            break;
        case ROW_AUTO_DIM:
            fmt_seconds(state_settings_auto_dim_ms(), buf, n);
            break;
        case ROW_RESET:
            snprintf(buf, n, "%s", "...");
            break;
        default: buf[0] = 0;
    }
}

static const char *row_label(row_t r) {
    switch (r) {
        case ROW_BRIGHTNESS: return "BRIGHT";
        case ROW_AUTO_SLEEP: return "SLEEP";
        case ROW_AUTO_DIM:   return "DIM";
        case ROW_RESET:      return "RESET";
        default: return "";
    }
}

static void draw_centered(const char *s, int y, int scale, uint16_t color) {
    int w = display_text_width(s, scale);
    display_text(s, (GFX_W - w) / 2, y, scale, color);
}

static void draw(void) {
    gfx_clear(C_BLACK);

    if (s_confirm_reset) {
        draw_centered("RESET",      40, 2, C_PINK);
        draw_centered("ALL?",       62, 2, C_PINK);
        draw_centered("PRESS = YES", 96, 1, C_LABEL);
        draw_centered("- = CANCEL", 110, 1, C_LABEL);
        gfx_present();
        return;
    }

    draw_centered("SETTINGS", 6, 2, C_LABEL);

    const int row_h = 22;
    const int top_y = 36;

    for (int i = 0; i < ROW__COUNT; i++) {
        int y = top_y + i * row_h;
        bool sel = (i == s_cursor);
        uint16_t fg = sel ? C_BLACK : C_WHITE;
        uint16_t bg = sel ? (s_editing ? C_ACCENT : C_LABEL) : C_BLACK;

        if (sel) gfx_fill_rect(2, y - 3, GFX_W - 4, row_h - 4, bg);
        display_text(row_label((row_t)i), 8, y, 1, fg);

        char val[16];
        row_value((row_t)i, val, sizeof(val));
        int vw = display_text_width(val, 1);
        display_text(val, GFX_W - 8 - vw, y, 1, fg);
    }

    gfx_present();
}

const program_t SETTINGS_PROGRAM = {
    .name           = "Settings",
    .on_enter       = on_enter,
    .on_rotate      = on_rotate,
    .on_short_press = on_short_press,
    .on_decrement   = on_decrement,
    .draw           = draw,
};
