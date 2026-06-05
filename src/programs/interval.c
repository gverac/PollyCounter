#include "interval.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "state.h"
#include "display.h"
#include "gfx.h"

typedef enum {
    PHASE_SETUP,
    PHASE_WORK,
    PHASE_REST,
    PHASE_PAUSED,
    PHASE_DONE,
} phase_t;

typedef enum {
    FIELD_WORK_MIN = 0,
    FIELD_WORK_SEC = 1,
    FIELD_REST_MIN = 2,
    FIELD_REST_SEC = 3,
    FIELD_ROUNDS   = 4,
    FIELD__COUNT,
} field_t;

static phase_t  s_phase;
static field_t  s_field;
static phase_t  s_pause_return;
static uint32_t s_work_s;
static uint32_t s_rest_s;
static uint32_t s_total_rounds;
static uint32_t s_round;
static int32_t  s_remaining_ms;
static uint32_t s_last_tick_ms;

static void draw(void);

static void load_from_state(void) {
    s_work_s       = state_interval_work_s();
    s_rest_s       = state_interval_rest_s();
    s_total_rounds = state_interval_rounds();
    if (s_work_s == 0)       s_work_s = 30;
    if (s_total_rounds == 0) s_total_rounds = 1;
}

static void on_enter(void) {
    load_from_state();
    s_phase = PHASE_SETUP;
    s_field = FIELD_WORK_MIN;
    s_round = 0;
    s_remaining_ms = 0;
    draw();
}

static void adjust_mmss(uint32_t *total_s, int delta, bool is_min, uint32_t min_s) {
    uint32_t mins = *total_s / 60;
    uint32_t secs = *total_s % 60;
    if (is_min) {
        int32_t v = (int32_t)mins + delta;
        if (v < 0)  v = 0;
        if (v > 99) v = 99;
        mins = (uint32_t)v;
    } else {
        int32_t v = (int32_t)secs + delta;
        while (v < 0)  v += 60;
        while (v > 59) v -= 60;
        secs = (uint32_t)v;
    }
    uint32_t t = mins * 60 + secs;
    if (t < min_s) t = min_s;
    if (t > 5999)  t = 5999;
    *total_s = t;
}

static void on_rotate(int delta) {
    if (s_phase != PHASE_SETUP) return;

    switch (s_field) {
        case FIELD_WORK_MIN: adjust_mmss(&s_work_s, delta, true,  1); break;
        case FIELD_WORK_SEC: adjust_mmss(&s_work_s, delta, false, 1); break;
        case FIELD_REST_MIN: adjust_mmss(&s_rest_s, delta, true,  0); break;
        case FIELD_REST_SEC: adjust_mmss(&s_rest_s, delta, false, 0); break;
        case FIELD_ROUNDS: {
            int32_t v = (int32_t)s_total_rounds + delta;
            if (v < 1)  v = 1;
            if (v > 99) v = 99;
            s_total_rounds = (uint32_t)v;
            break;
        }
        default: break;
    }
    draw();
}

static void on_short_press(void) {
    if (s_phase == PHASE_SETUP) {
        s_field = (field_t)((s_field + 1) % FIELD__COUNT);
    } else {
        state_interval_set_work_s(s_work_s);
        state_interval_set_rest_s(s_rest_s);
        state_interval_set_rounds(s_total_rounds);
        s_phase = PHASE_SETUP;
        s_field = FIELD_WORK_MIN;
    }
    draw();
}

static void start_phase(phase_t p) {
    s_phase = p;
    s_remaining_ms = (int32_t)((p == PHASE_WORK ? s_work_s : s_rest_s) * 1000);
    s_last_tick_ms = to_ms_since_boot(get_absolute_time());
}

static void on_increment(void) {
    if (s_phase == PHASE_SETUP) {
        state_interval_set_work_s(s_work_s);
        state_interval_set_rest_s(s_rest_s);
        state_interval_set_rounds(s_total_rounds);
        s_round = 1;
        start_phase(PHASE_WORK);
    } else if (s_phase == PHASE_PAUSED) {
        s_phase = s_pause_return;
        s_last_tick_ms = to_ms_since_boot(get_absolute_time());
    } else if (s_phase == PHASE_DONE) {
        s_round = 1;
        start_phase(PHASE_WORK);
    }
    draw();
}

static void on_decrement(void) {
    if (s_phase == PHASE_WORK || s_phase == PHASE_REST) {
        s_pause_return = s_phase;
        s_phase = PHASE_PAUSED;
        draw();
    } else if (s_phase == PHASE_PAUSED) {
        s_phase = s_pause_return;
        s_last_tick_ms = to_ms_since_boot(get_absolute_time());
        draw();
    } else if (s_phase == PHASE_DONE) {
        s_phase = PHASE_SETUP;
        draw();
    }
}

static void advance(void) {
    if (s_phase == PHASE_WORK) {
        if (s_rest_s > 0 && s_round < s_total_rounds) {
            start_phase(PHASE_REST);
        } else if (s_round < s_total_rounds) {
            s_round++;
            start_phase(PHASE_WORK);
        } else {
            s_phase = PHASE_DONE;
        }
    } else {  // PHASE_REST
        if (s_round < s_total_rounds) {
            s_round++;
            start_phase(PHASE_WORK);
        } else {
            s_phase = PHASE_DONE;
        }
    }
}

static void tick(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (s_phase == PHASE_WORK || s_phase == PHASE_REST) {
        uint32_t elapsed = now - s_last_tick_ms;
        s_last_tick_ms = now;
        s_remaining_ms -= (int32_t)elapsed;
        if (s_remaining_ms <= 0) {
            advance();
        }
        draw();
    } else if (s_phase == PHASE_SETUP) {
        static uint32_t last_blink = 0;
        if (now - last_blink >= 250) { last_blink = now; draw(); }
    }
}

// ── Drawing ────────────────────────────────────────────────────────────────

static void draw_centered(const char *s, int y, int scale, uint16_t color) {
    int w = display_text_width(s, scale);
    display_text(s, (GFX_W - w) / 2, y, scale, color);
}

static void format_mmss(int32_t ms, char *buf, size_t n) {
    if (ms < 0) ms = 0;
    uint32_t total_s = (uint32_t)(ms / 1000);
    snprintf(buf, n, "%02lu:%02lu",
             (unsigned long)(total_s / 60),
             (unsigned long)(total_s % 60));
}

// Draw a "WORK 00:30" row. Underlines either the minutes or the seconds when
// the row owns the active field; or the whole number for the rounds row.
static void draw_setup_row(int y,
                           const char *label,
                           uint32_t total_s,
                           bool is_rounds,
                           field_t my_min_field,
                           field_t my_sec_field,
                           bool blink_on) {
    bool sel_min = (s_field == my_min_field);
    bool sel_sec = (s_field == my_sec_field);
    bool any_sel = sel_min || sel_sec;

    uint16_t label_color = any_sel ? C_ACCENT : C_LABEL;
    display_text(label, 4, y + 4, 1, label_color);

    char buf[16];
    int scale = 2;
    if (is_rounds) {
        snprintf(buf, sizeof(buf), "%02lu", (unsigned long)total_s);
    } else {
        format_mmss((int32_t)(total_s * 1000), buf, sizeof(buf));
    }
    int w = display_text_width(buf, scale);
    int x = GFX_W - 4 - w;
    uint16_t value_color = any_sel ? C_ACCENT : C_WHITE;
    display_text(buf, x, y, scale, value_color);

    if (any_sel && blink_on) {
        int ch_w = 8 * scale;
        int uy = y + 8 * scale + 2;
        int seg_w = 2 * ch_w;
        int seg_x;
        if (is_rounds) {
            seg_x = x;
        } else if (sel_min) {
            seg_x = x;            // chars 0..1
        } else {
            seg_x = x + 3 * ch_w; // chars 3..4 (skip the colon)
        }
        gfx_fill_rect(seg_x, uy, seg_w, 2, C_ACCENT);
    }
}

static void draw_setup(void) {
    gfx_clear(C_BLACK);
    draw_centered("INTERVAL", 6, 2, C_LABEL);

    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool blink_on = ((now / 400) & 1) == 0;

    const int row_h = 26;
    const int top_y = 36;

    draw_setup_row(top_y + 0 * row_h, "WORK",   s_work_s,
                   false, FIELD_WORK_MIN, FIELD_WORK_SEC, blink_on);
    draw_setup_row(top_y + 1 * row_h, "REST",   s_rest_s,
                   false, FIELD_REST_MIN, FIELD_REST_SEC, blink_on);
    draw_setup_row(top_y + 2 * row_h, "RNDS",   s_total_rounds,
                   true,  FIELD_ROUNDS,   FIELD_ROUNDS,   blink_on);

    draw_centered("o NEXT FIELD", 126, 1, C_LABEL);
    draw_centered("+ START",      140, 1, C_LABEL);
    gfx_present();
}

static void draw_run(void) {
    gfx_clear(C_BLACK);

    const char *title = "";
    uint16_t color = C_WHITE;
    switch (s_phase) {
        case PHASE_WORK:   title = "WORK";   color = C_MINT;   break;
        case PHASE_REST:   title = "REST";   color = C_YELLOW; break;
        case PHASE_PAUSED: title = "PAUSED"; color = C_YELLOW; break;
        case PHASE_DONE:   title = "DONE!";  color = C_PINK;   break;
        default: break;
    }
    draw_centered(title, 8, 2, color);

    char buf[16];
    if (s_phase != PHASE_DONE) {
        snprintf(buf, sizeof(buf), "%lu/%lu",
                 (unsigned long)s_round, (unsigned long)s_total_rounds);
        draw_centered(buf, 32, 1, C_LABEL);
    }

    format_mmss(s_phase == PHASE_DONE ? 0 : s_remaining_ms, buf, sizeof(buf));
    int scale = 3;
    int w = display_text_width(buf, scale);
    while (w > GFX_W - 4 && scale > 1) { scale--; w = display_text_width(buf, scale); }
    display_text(buf, (GFX_W - w) / 2, 60, scale, color);

    const char *h1 = NULL, *h2 = NULL;
    switch (s_phase) {
        case PHASE_WORK:
        case PHASE_REST:   h1 = "- PAUSE";    h2 = "o RE-SET"; break;
        case PHASE_PAUSED: h1 = "+/- RESUME"; h2 = "o RE-SET"; break;
        case PHASE_DONE:   h1 = "+ RESTART";  h2 = "- DISMISS"; break;
        default: break;
    }
    if (h1) draw_centered(h1, 134, 1, C_LABEL);
    if (h2) draw_centered(h2, 146, 1, C_LABEL);

    gfx_present();
}

static void draw(void) {
    if (s_phase == PHASE_SETUP) draw_setup();
    else                        draw_run();
}

const program_t INTERVAL_PROGRAM = {
    .name           = "Interval",
    .on_enter       = on_enter,
    .on_rotate      = on_rotate,
    .on_short_press = on_short_press,
    .on_increment   = on_increment,
    .on_decrement   = on_decrement,
    .tick           = tick,
    .draw           = draw,
};
