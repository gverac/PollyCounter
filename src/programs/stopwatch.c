#include "stopwatch.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "display.h"
#include "gfx.h"

// Stopwatch shows mm:ss.cs (centi-seconds). Internally we accumulate ms.

static bool     s_running;
static uint32_t s_elapsed_ms;     // total elapsed when stopped
static uint32_t s_run_start_ms;   // wall-clock at last start
static int32_t  s_last_lap_ms;    // -1 = no lap yet

static uint32_t s_last_redraw_ms;

static void draw(void);

static uint32_t now_elapsed(void) {
    if (!s_running) return s_elapsed_ms;
    return s_elapsed_ms + (to_ms_since_boot(get_absolute_time()) - s_run_start_ms);
}

static void on_enter(void) {
    s_running = false;
    s_elapsed_ms = 0;
    s_last_lap_ms = -1;
    s_last_redraw_ms = 0;
    draw();
}

static void on_increment(void) {
    if (s_running) {
        s_elapsed_ms += to_ms_since_boot(get_absolute_time()) - s_run_start_ms;
        s_running = false;
    } else {
        s_run_start_ms = to_ms_since_boot(get_absolute_time());
        s_running = true;
    }
    draw();
}

static void on_decrement(void) {
    // Reset only when stopped, to avoid accidental loss of in-progress timing.
    if (!s_running) {
        s_elapsed_ms = 0;
        s_last_lap_ms = -1;
        draw();
    }
}

static void on_short_press(void) {
    if (s_running) {
        s_last_lap_ms = (int32_t)now_elapsed();
        draw();
    }
}

static void tick(void) {
    if (!s_running) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - s_last_redraw_ms >= 50) {     // ~20 fps refresh
        s_last_redraw_ms = now;
        draw();
    }
}

// ── Drawing ────────────────────────────────────────────────────────────────

static void draw_centered(const char *s, int y, int scale, uint16_t color) {
    int w = display_text_width(s, scale);
    display_text(s, (GFX_W - w) / 2, y, scale, color);
}

static void format_time(uint32_t ms, char *buf, size_t n) {
    uint32_t cs    = (ms / 10) % 100;
    uint32_t total_s = ms / 1000;
    snprintf(buf, n, "%02lu:%02lu.%02lu",
             (unsigned long)(total_s / 60),
             (unsigned long)(total_s % 60),
             (unsigned long)cs);
}

static void draw(void) {
    gfx_clear(C_BLACK);

    draw_centered("STOPWATCH", 6, 1, C_LABEL);

    char buf[16];
    format_time(now_elapsed(), buf, sizeof(buf));
    // mm:ss.cs is 8 chars wide; at scale 2 that's exactly 128 px. Auto-shrink
    // if a future format ever overflows.
    int scale = 2;
    int w = display_text_width(buf, scale);
    while (w > GFX_W && scale > 1) { scale--; w = display_text_width(buf, scale); }
    display_text(buf, (GFX_W - w) / 2, 36, scale, s_running ? C_MINT : C_WHITE);

    if (s_last_lap_ms >= 0) {
        draw_centered("LAP", 86, 1, C_LABEL);
        format_time((uint32_t)s_last_lap_ms, buf, sizeof(buf));
        scale = 2;
        w = display_text_width(buf, scale);
        display_text(buf, (GFX_W - w) / 2, 100, scale, C_ACCENT);
    }

    const char *h1 = s_running ? "+ STOP"  : "+ START";
    const char *h2 = s_running ? "o LAP"   : "- RESET";
    draw_centered(h1, 134, 1, C_LABEL);
    draw_centered(h2, 146, 1, C_LABEL);

    gfx_present();
}

const program_t STOPWATCH_PROGRAM = {
    .name           = "Stopwatch",
    .on_enter       = on_enter,
    .on_short_press = on_short_press,
    .on_increment   = on_increment,
    .on_decrement   = on_decrement,
    .tick           = tick,
    .draw           = draw,
};
