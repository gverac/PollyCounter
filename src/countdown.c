#include "countdown.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "state.h"
#include "display.h"
#include "gfx.h"

typedef enum {
    PHASE_SETUP,    // rotate adjusts minutes or seconds (toggle via press)
    PHASE_IDLE,     // configured, not running. + starts.
    PHASE_RUNNING,  // counting down
    PHASE_PAUSED,
    PHASE_DONE,     // finished, showing message
} phase_t;

typedef enum { EDIT_SEC = 0, EDIT_MIN = 1 } edit_field_t;

static phase_t      s_phase;
static edit_field_t s_field;
static uint32_t     s_total_s;       // configured total seconds
static int32_t      s_remaining_ms;  // ms remaining
static uint32_t     s_last_tick_ms;  // wall-clock ms at last tick

static void draw(void);

static void on_enter(void) {
    s_total_s = state_countdown_get_seconds();
    if (s_total_s == 0) s_total_s = 30;
    s_remaining_ms = (int32_t)(s_total_s * 1000);
    s_phase = PHASE_SETUP;
    s_field = EDIT_SEC;
    draw();
}

static void enter_setup(void) {
    state_countdown_set_seconds(s_total_s);
    s_phase = PHASE_SETUP;
    s_field = EDIT_SEC;
    s_remaining_ms = (int32_t)(s_total_s * 1000);
}

static void on_rotate(int delta) {
    if (s_phase != PHASE_SETUP) return;

    uint32_t mins = s_total_s / 60;
    uint32_t secs = s_total_s % 60;

    if (s_field == EDIT_SEC) {
        int32_t v = (int32_t)secs + delta;
        while (v < 0)  v += 60;
        while (v > 59) v -= 60;
        secs = (uint32_t)v;
    } else {
        int32_t v = (int32_t)mins + delta;
        if (v < 0)  v = 0;
        if (v > 99) v = 99;
        mins = (uint32_t)v;
    }
    s_total_s = mins * 60 + secs;
    if (s_total_s < 1) s_total_s = 1;
    s_remaining_ms = (int32_t)(s_total_s * 1000);
    draw();
}

static void on_short_press(void) {
    if (s_phase == PHASE_SETUP) {
        // Toggle which field rotation edits.
        s_field = (s_field == EDIT_SEC) ? EDIT_MIN : EDIT_SEC;
        draw();
    } else {
        enter_setup();
        draw();
    }
}

static void start_running(void) {
    s_last_tick_ms = to_ms_since_boot(get_absolute_time());
    s_phase = PHASE_RUNNING;
}

static void on_increment(void) {
    if (s_phase == PHASE_SETUP) {
        state_countdown_set_seconds(s_total_s);
        s_remaining_ms = (int32_t)(s_total_s * 1000);
        start_running();
    } else if (s_phase == PHASE_IDLE) {
        s_remaining_ms = (int32_t)(s_total_s * 1000);
        start_running();
    } else if (s_phase == PHASE_PAUSED) {
        start_running();
    } else if (s_phase == PHASE_DONE) {
        s_remaining_ms = (int32_t)(s_total_s * 1000);
        start_running();
    }
    draw();
}

static void on_decrement(void) {
    if (s_phase == PHASE_RUNNING) {
        s_phase = PHASE_PAUSED;
        draw();
    } else if (s_phase == PHASE_PAUSED) {
        start_running();
        draw();
    } else if (s_phase == PHASE_DONE) {
        // Acknowledge — return to idle, ready to restart with +.
        s_remaining_ms = (int32_t)(s_total_s * 1000);
        s_phase = PHASE_IDLE;
        draw();
    }
}

static void tick(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (s_phase == PHASE_RUNNING) {
        uint32_t elapsed = now - s_last_tick_ms;
        s_last_tick_ms = now;
        s_remaining_ms -= (int32_t)elapsed;
        if (s_remaining_ms <= 0) {
            s_remaining_ms = 0;
            s_phase = PHASE_DONE;
        }
        draw();
    } else if (s_phase == PHASE_SETUP) {
        // Drive the blinking underline.
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

static void draw_hint(const char *l1, const char *l2) {
    if (l1) {
        int w = display_text_width(l1, 1);
        display_text(l1, (GFX_W - w) / 2, 134, 1, C_LABEL);
    }
    if (l2) {
        int w = display_text_width(l2, 1);
        display_text(l2, (GFX_W - w) / 2, 146, 1, C_LABEL);
    }
}

static void draw(void) {
    gfx_clear(C_BLACK);

    const char *title = "TIMER";
    uint16_t color    = C_WHITE;
    const char *h1 = NULL, *h2 = NULL;

    switch (s_phase) {
        case PHASE_SETUP:
            title = "SET TIME"; color = C_ACCENT;
            h1 = "o SWITCH FIELD"; h2 = "+ START";
            break;
        case PHASE_IDLE:
            title = "READY"; color = C_WHITE;
            h1 = "+ START"; h2 = "o RE-SET";
            break;
        case PHASE_RUNNING:
            title = "RUNNING"; color = C_MINT;
            h1 = "- PAUSE"; h2 = "o RE-SET";
            break;
        case PHASE_PAUSED:
            title = "PAUSED"; color = C_YELLOW;
            h1 = "+/- RESUME"; h2 = "o RE-SET";
            break;
        case PHASE_DONE:
            title = "DONE!"; color = C_PINK;
            h1 = "+ RESTART"; h2 = "- DISMISS";
            break;
    }

    draw_centered(title, 10, 2, color);

    char buf[16];
    format_mmss(s_remaining_ms, buf, sizeof(buf));
    int scale = 3;
    int w = display_text_width(buf, scale);
    while (w > GFX_W - 4 && scale > 1) { scale--; w = display_text_width(buf, scale); }
    int x = (GFX_W - w) / 2;
    int y = 60;
    display_text(buf, x, y, scale, color);

    // Underline for the active field in setup mode (blinking).
    if (s_phase == PHASE_SETUP) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        bool on = ((now / 400) & 1) == 0;
        if (on) {
            int ch_w = 8 * scale;
            int field_w = 2 * ch_w;
            int field_x = (s_field == EDIT_MIN) ? x : (x + 3 * ch_w);
            int uy = y + 8 * scale + 2;
            gfx_fill_rect(field_x, uy, field_w, 3, C_ACCENT);
        }
    }

    draw_hint(h1, h2);

    gfx_present();
}

const program_t COUNTDOWN_PROGRAM = {
    .name           = "Countdown",
    .on_enter       = on_enter,
    .on_rotate      = on_rotate,
    .on_short_press = on_short_press,
    .on_increment   = on_increment,
    .on_decrement   = on_decrement,
    .tick           = tick,
    .draw           = draw,
};
