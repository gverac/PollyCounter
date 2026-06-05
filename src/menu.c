#include "menu.h"

#include "display.h"
#include "gfx.h"

typedef enum {
    ITEM_SLEEP,
    ITEM_COUNTER,
    ITEM_REP,
    ITEM_COUNTDOWN,
    ITEM_STOPWATCH,
    ITEM_SETTINGS,
    ITEM_FLASH,
    ITEM__COUNT
} item_t;

static const char *ITEM_LABEL[ITEM__COUNT] = {
    "SLEEP",
    "COUNTER",
    "REP COUNTER",
    "COUNTDOWN",
    "STOPWATCH",
    "SETTINGS",
    "FLASH MODE",
};

static int s_cursor = ITEM_SLEEP;

void menu_reset_cursor(void) { s_cursor = ITEM_SLEEP; }

static void draw(void);

static void on_enter(void) { draw(); }

static void on_rotate(int delta) {
    int v = s_cursor + delta;
    if (v < 0) v = 0;
    if (v >= ITEM__COUNT) v = ITEM__COUNT - 1;
    s_cursor = v;
    draw();
}

static void on_short_press(void) {
    switch ((item_t)s_cursor) {
        case ITEM_SLEEP:     app_request_sleep();                 break;
        case ITEM_COUNTER:   app_switch_to(PROGRAM_COUNTER);      break;
        case ITEM_REP:       app_switch_to(PROGRAM_REP);          break;
        case ITEM_COUNTDOWN: app_switch_to(PROGRAM_COUNTDOWN);    break;
        case ITEM_STOPWATCH: app_switch_to(PROGRAM_STOPWATCH);    break;
        case ITEM_SETTINGS:  app_switch_to(PROGRAM_SETTINGS);     break;
        case ITEM_FLASH:     app_request_flash_mode();            break;
        default: break;
    }
}

static void on_long_press(void) {
    // Long press in the menu closes it (returns to previous program).
    app_close_menu();
}

// ── Drawing ────────────────────────────────────────────────────────────────

static void draw_centered(const char *s, int y, int scale, uint16_t color) {
    int w = display_text_width(s, scale);
    display_text(s, (GFX_W - w) / 2, y, scale, color);
}

static void draw(void) {
    gfx_clear(C_BLACK);
    draw_centered("MENU", 6, 2, C_LABEL);

    const int row_h = 18;
    const int top_y = 32;

    // Scroll so the cursor is visible (small viewport: 7 rows fit).
    const int visible = 7;
    int first = 0;
    if (ITEM__COUNT > visible) {
        first = s_cursor - visible / 2;
        if (first < 0) first = 0;
        if (first > ITEM__COUNT - visible) first = ITEM__COUNT - visible;
    }

    for (int i = 0; i < visible && (first + i) < ITEM__COUNT; i++) {
        int idx = first + i;
        int y = top_y + i * row_h;
        bool sel = (idx == s_cursor);
        if (sel) {
            gfx_fill_rect(2, y - 3, GFX_W - 4, row_h - 4, C_ACCENT);
        }
        const char *label = ITEM_LABEL[idx];
        int tw = display_text_width(label, 1);
        display_text(label, (GFX_W - tw) / 2, y,
                     1, sel ? C_BLACK : C_WHITE);
    }

    gfx_present();
}

const program_t MENU_PROGRAM = {
    .name           = "Menu",
    .on_enter       = on_enter,
    .on_rotate      = on_rotate,
    .on_short_press = on_short_press,
    .on_long_press  = on_long_press,
    .draw           = draw,
};
