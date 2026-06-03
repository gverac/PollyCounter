#include "display.h"
#include "font8x8.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"

// Pins
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_RST  20
#define PIN_DC   21
#define PIN_CS   17

#define SPI_PORT spi0
#define SPI_BAUD (32 * 1000 * 1000)   // 32 MHz; ST7735 spec'd to 15-32 MHz

// ST7735 commands
#define ST_SWRESET 0x01
#define ST_SLPOUT  0x11
#define ST_NORON   0x13
#define ST_INVOFF  0x20
#define ST_DISPON  0x29
#define ST_CASET   0x2A
#define ST_RASET   0x2B
#define ST_RAMWR   0x2C
#define ST_MADCTL  0x36
#define ST_COLMOD  0x3A

static inline void cs_select(void)   { gpio_put(PIN_CS, 0); }
static inline void cs_deselect(void) { gpio_put(PIN_CS, 1); }

static void write_cmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    cs_select();
    spi_write_blocking(SPI_PORT, &cmd, 1);
    cs_deselect();
}

static void write_data(const uint8_t *buf, size_t len) {
    gpio_put(PIN_DC, 1);
    cs_select();
    spi_write_blocking(SPI_PORT, buf, len);
    cs_deselect();
}

static void write_data_byte(uint8_t b) {
    write_data(&b, 1);
}

static void set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    uint8_t col[] = {0x00, x0, 0x00, x1};
    uint8_t row[] = {0x00, y0, 0x00, y1};
    write_cmd(ST_CASET);
    write_data(col, 4);
    write_cmd(ST_RASET);
    write_data(row, 4);
    write_cmd(ST_RAMWR);
}

void display_init(void) {
    // SPI init
    spi_init(SPI_PORT, SPI_BAUD);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // CS / DC / RST as GPIO outs
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC,  GPIO_OUT); gpio_put(PIN_DC,  0);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);

    // Hardware reset
    sleep_ms(10);
    gpio_put(PIN_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);

    // Init sequence
    write_cmd(ST_SWRESET);    sleep_ms(150);
    write_cmd(ST_SLPOUT);     sleep_ms(500);
    write_cmd(ST_COLMOD);     write_data_byte(0x05);   // 16-bit color
    write_cmd(ST_MADCTL);     write_data_byte(0x00);   // row/col order
    write_cmd(ST_INVOFF);
    write_cmd(ST_NORON);      sleep_ms(10);
    write_cmd(ST_DISPON);     sleep_ms(100);

    display_clear(COLOR_BLACK);
}

static void fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0 || y < 0 || x + w > DISPLAY_WIDTH || y + h > DISPLAY_HEIGHT) return;

    set_window(x, y, x + w - 1, y + h - 1);

    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;
    uint8_t chunk[64];
    for (int i = 0; i < 32; i++) {
        chunk[2*i]     = hi;
        chunk[2*i + 1] = lo;
    }

    gpio_put(PIN_DC, 1);
    cs_select();
    int total = w * h;
    while (total >= 32) {
        spi_write_blocking(SPI_PORT, chunk, 64);
        total -= 32;
    }
    if (total > 0) {
        spi_write_blocking(SPI_PORT, chunk, total * 2);
    }
    cs_deselect();
}

void display_clear(uint16_t color) {
    fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

// ── Full-frame DMA blit ─────────────────────────────────────────────────────

static int s_dma_chan = -1;

void display_blit_full(const uint16_t *fb) {
    if (s_dma_chan < 0) {
        s_dma_chan = dma_claim_unused_channel(true);
    }

    set_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    gpio_put(PIN_DC, 1);
    cs_select();

    // ST7735 is big-endian for RGB565 over SPI. Configure SPI to 16-bit so
    // each halfword goes out high-byte first.
    spi_set_format(SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    dma_channel_config c = dma_channel_get_default_config(s_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(
        s_dma_chan, &c,
        &spi_get_hw(SPI_PORT)->dr,         // dest: SPI data register
        fb,                                 // src: framebuffer
        DISPLAY_WIDTH * DISPLAY_HEIGHT,    // count of halfwords
        true                                // start now
    );

    dma_channel_wait_for_finish_blocking(s_dma_chan);
    // Wait for SPI FIFO to drain before deasserting CS.
    while (spi_is_busy(SPI_PORT)) tight_loop_contents();

    // Restore 8-bit SPI for the rest of the driver.
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    cs_deselect();
}

// ── Framebuffer-based text & layout ─────────────────────────────────────────
//
// All on-screen rendering now goes through the gfx layer (which writes into a
// 128x160 RGB565 framebuffer). display_blit_full() pushes the FB to the panel.

#include "gfx.h"

// ── Tamagotchi-style colors ─────────────────────────────────────────────────

#define C_PASTEL_BG    0xFCD0   // deeper, more saturated tamagotchi pink
#define C_PASTEL_INK   0x2086   // very dark navy/plum — high contrast on pink
#define C_DIM_SEGMENT  0x18C3   // dim section of progress bar
#define C_LIVE_SEGMENT 0xFFFF   // lit section

// Particle colors — saturated, bold, picked to pop against the pink bg.
static const uint16_t SPARK_COLORS[] = {
    0x001F,  // electric blue
    0xF81F,  // magenta
    0x07FF,  // cyan
    0x07E0,  // pure lime
    0xFC00,  // hot orange
    0x4810,  // deep purple
};
#define SPARK_COLORS_N (sizeof(SPARK_COLORS) / sizeof(SPARK_COLORS[0]))

// ── 8x8 pixel font, scaled ──────────────────────────────────────────────────

static void fb_8x8_char_scaled(char c, int x, int y, int scale, uint16_t color) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *g = FONT8X8[c - 32];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            if ((bits >> col) & 1) {
                // Each source pixel becomes a scale×scale block.
                gfx_fill_rect(x + col * scale, y + row * scale,
                              scale, scale, color);
            }
        }
    }
}

static void fb_8x8_text_scaled(const char *s, int x, int y, int scale, uint16_t color) {
    int cw = 8 * scale;
    while (*s) {
        fb_8x8_char_scaled(*s, x, y, scale, color);
        x += cw;
        s++;
    }
}

static int fb_8x8_text_w_scaled(const char *s, int scale) {
    return (int)strlen(s) * 8 * scale;
}

// Convenience for scale-1 calls (sleep messages, debug)
static void fb_8x8_text(const char *s, int x, int y, uint16_t color) {
    fb_8x8_text_scaled(s, x, y, 1, color);
}
static int fb_8x8_text_w(const char *s) { return (int)strlen(s) * 8; }

// ── Continuous tamagotchi progress bar ──────────────────────────────────────

static void fb_continuous_progress(uint32_t count, uint32_t target, int y,
                                    uint16_t lit, uint16_t dim, uint16_t border) {
    const int bar_w = 108;
    const int bar_h = 12;
    const int bar_x = (DISPLAY_WIDTH - bar_w) / 2;

    // 1px border, dim interior
    gfx_fill_rect(bar_x, y, bar_w, bar_h, border);
    gfx_fill_rect(bar_x + 1, y + 1, bar_w - 2, bar_h - 2, dim);

    if (target == 0 || count == 0) return;

    // Fill grows continuously from left
    uint32_t fw = ((uint64_t)(bar_w - 2) * count) / target;
    if (fw > (uint32_t)(bar_w - 2)) fw = bar_w - 2;
    if (fw > 0) {
        gfx_fill_rect(bar_x + 1, y + 1, (int)fw, bar_h - 2, lit);
    }
}

// ── Fireworks ───────────────────────────────────────────────────────────────
//
// Particle state machine. Triggered when count transitions to >= target.
// Plays ~4 bursts over CELEBRATE_MS, then settles into the static "hit" screen.

#define PARTICLE_MAX        72
#define CELEBRATE_MS        3000
#define BURST_INTERVAL_MS    650
#define PARTICLES_PER_BURST  18
#define GRAVITY_Q8           14    // ~0.05 px/frame² in 8.8 fixed point
#define FRAME_TARGET_MS      55

typedef struct {
    int16_t  x, y;       // 8.8 fixed-point position
    int16_t  vx, vy;     // 8.8 fixed-point velocity (px/frame)
    uint8_t  life;       // frames remaining; 0 = dead
    uint16_t color;
} particle_t;

static particle_t s_particles[PARTICLE_MAX];

static bool     s_celebrating       = false;
static bool     s_was_match         = false;
static uint32_t s_celebrate_start   = 0;
static uint32_t s_last_frame_ms     = 0;
static uint8_t  s_bursts_fired      = 0;

// 16-direction unit-ish vectors scaled by 256 (sin/cos LUT)
static const int16_t COS16[16] = {
     256,  237,  181,   98,    0,  -98, -181, -237,
    -256, -237, -181,  -98,    0,   98,  181,  237,
};
static const int16_t SIN16[16] = {
       0,   98,  181,  237,  256,  237,  181,   98,
       0,  -98, -181, -237, -256, -237, -181,  -98,
};

// Tiny xorshift32 PRNG — deterministic seed reseeded from time at boot
static uint32_t s_rng = 0xC0FFEEu;
static inline uint32_t rng_next(void) {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static void spawn_burst(int cx, int cy) {
    // Pick a base speed per burst so bursts feel uniform-ish
    int speed = 240 + (int)(rng_next() % 180);   // ~1..1.6 px/frame
    uint16_t color = SPARK_COLORS[rng_next() % SPARK_COLORS_N];

    int spawned = 0;
    for (int i = 0; i < PARTICLE_MAX && spawned < PARTICLES_PER_BURST; i++) {
        if (s_particles[i].life != 0) continue;
        int angle = (int)(rng_next() & 0x0F);    // 0..15
        // Slight per-particle speed jitter
        int sp = speed + (int)(rng_next() % 80) - 40;
        s_particles[i].x  = cx << 8;
        s_particles[i].y  = cy << 8;
        s_particles[i].vx = (int16_t)((COS16[angle] * sp) >> 8);
        s_particles[i].vy = (int16_t)((SIN16[angle] * sp) >> 8);
        s_particles[i].life  = 28 + (uint8_t)(rng_next() % 12);
        s_particles[i].color = color;
        spawned++;
    }
}

static void update_particles(void) {
    for (int i = 0; i < PARTICLE_MAX; i++) {
        if (s_particles[i].life == 0) continue;
        s_particles[i].x  += s_particles[i].vx;
        s_particles[i].y  += s_particles[i].vy;
        s_particles[i].vy += GRAVITY_Q8;
        s_particles[i].life--;
    }
}

static void draw_particles(void) {
    for (int i = 0; i < PARTICLE_MAX; i++) {
        uint8_t life = s_particles[i].life;
        if (life == 0) continue;
        int px = s_particles[i].x >> 8;
        int py = s_particles[i].y >> 8;
        // Late-life: shrink to 1×1; early-life: chunky 2×2 — tamagotchi pixel
        if (life > 8) gfx_fill_rect(px, py, 2, 2, s_particles[i].color);
        else          gfx_px(px, py, s_particles[i].color);
    }
}

static void reset_particles(void) {
    for (int i = 0; i < PARTICLE_MAX; i++) s_particles[i].life = 0;
}

// ── Scene drawing ───────────────────────────────────────────────────────────

static void draw_scene(uint32_t count, uint32_t target, bool hit) {
    uint16_t bg      = hit ? C_PASTEL_BG  : C_BLACK;
    uint16_t ink     = hit ? C_PASTEL_INK : C_WHITE;
    uint16_t ink_dim = hit ? 0x73AE       : 0x8410;   // greyer ink for label + goal
    uint16_t bar_lit = hit ? C_PASTEL_INK : C_LIVE_SEGMENT;
    uint16_t bar_dim = hit ? 0xFBB6       : C_DIM_SEGMENT;
    uint16_t bar_brd = hit ? C_PASTEL_INK : C_WHITE;

    gfx_clear(bg);

    // Counter — 8x8 font at scale 5: 3 digits * 8 * 5 = 120 wide, 40 tall.
    char buf[8];
    snprintf(buf, sizeof(buf), "%03lu",
             (unsigned long)(count > 999 ? 999 : count));
    int cw = fb_8x8_text_w_scaled(buf, 5);
    fb_8x8_text_scaled(buf, (DISPLAY_WIDTH - cw) / 2, 12, 5, ink);

    // "GOAL" label scale 2: 4 chars * 16 = 64 wide
    fb_8x8_text_scaled("GOAL", (DISPLAY_WIDTH - 4 * 16) / 2, 66, 2, ink_dim);

    // Target number scale 3: 3 chars * 24 = 72 wide
    snprintf(buf, sizeof(buf), "%03lu",
             (unsigned long)(target > 999 ? 999 : target));
    int tw = fb_8x8_text_w_scaled(buf, 3);
    fb_8x8_text_scaled(buf, (DISPLAY_WIDTH - tw) / 2, 88, 3, ink_dim);

    // Continuous progress bar
    fb_continuous_progress(count, target, 128, bar_lit, bar_dim, bar_brd);
}

// ── Public API ──────────────────────────────────────────────────────────────

// Cached state so display_tick can repaint during the celebration animation
// without main needing to pass count/target every frame.
static uint32_t s_cur_count  = 0;
static uint32_t s_cur_target = 0;

void display_draw_screen(uint32_t count, uint32_t target) {
    s_cur_count  = count;
    s_cur_target = target;
    bool match = (target > 0 && count == target);

    // Trigger celebration only on transition into the exact-match state.
    if (match && !s_was_match) {
        s_celebrating     = true;
        s_celebrate_start = to_ms_since_boot(get_absolute_time());
        s_bursts_fired    = 0;
        s_rng ^= s_celebrate_start;   // reseed per session
        reset_particles();
        // Fire the first burst immediately so the screen pops.
        int cx = 30 + (int)(rng_next() % 70);
        int cy = 30 + (int)(rng_next() % 80);
        spawn_burst(cx, cy);
        s_bursts_fired = 1;
    }
    // Drift away from match (either direction): cut the celebration immediately.
    if (!match && s_celebrating) {
        s_celebrating = false;
        reset_particles();
    }
    s_was_match = match;

    draw_scene(count, target, match);
    if (s_celebrating) draw_particles();
    gfx_present();
}

void display_tick(void) {
    if (!s_celebrating) return;

    // Safety: if anything ticks while we've drifted off-match, stop.
    bool match = (s_cur_target > 0 && s_cur_count == s_cur_target);
    if (!match) {
        s_celebrating = false;
        reset_particles();
        draw_scene(s_cur_count, s_cur_target, false);
        gfx_present();
        return;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - s_last_frame_ms < FRAME_TARGET_MS) return;
    s_last_frame_ms = now;

    uint32_t elapsed = now - s_celebrate_start;

    // Spawn additional bursts at fixed intervals
    uint8_t want_bursts = (uint8_t)(elapsed / BURST_INTERVAL_MS) + 1;
    while (s_bursts_fired < want_bursts && elapsed < CELEBRATE_MS - 400) {
        int cx = 20 + (int)(rng_next() % 88);
        int cy = 20 + (int)(rng_next() % 90);
        spawn_burst(cx, cy);
        s_bursts_fired++;
    }

    update_particles();

    if (elapsed >= CELEBRATE_MS) {
        // End celebration only once all particles have died out.
        bool any_alive = false;
        for (int i = 0; i < PARTICLE_MAX; i++) {
            if (s_particles[i].life != 0) { any_alive = true; break; }
        }
        if (!any_alive) {
            s_celebrating = false;
            draw_scene(s_cur_count, s_cur_target, true);
            gfx_present();
            return;
        }
    }

    draw_scene(s_cur_count, s_cur_target, true);
    draw_particles();
    gfx_present();
}

void display_update_count(uint32_t count, uint32_t target) {
    display_draw_screen(count, target);
}

void display_update_target(uint32_t count, uint32_t target) {
    display_draw_screen(count, target);
}

void display_show_message(const char *line1, const char *line2, uint16_t color) {
    gfx_clear(C_BLACK);
    int w1 = fb_8x8_text_w(line1);
    fb_8x8_text(line1, (DISPLAY_WIDTH - w1) / 2, 70, color);
    if (line2 && *line2) {
        int w2 = fb_8x8_text_w(line2);
        fb_8x8_text(line2, (DISPLAY_WIDTH - w2) / 2, 88, 0x8410);
    }
    gfx_present();
}
