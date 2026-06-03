#include "gfx.h"

#include <string.h>

#include "display.h"

// ── Framebuffer ─────────────────────────────────────────────────────────────

static uint16_t s_fb[GFX_W * GFX_H];

static inline bool in_bounds(int x, int y) {
    return (unsigned)x < GFX_W && (unsigned)y < GFX_H;
}

void gfx_init(void) {
    memset(s_fb, 0, sizeof(s_fb));
}

void gfx_present(void) {
    display_blit_full(s_fb);
}

// ── Pixel + alpha blend ─────────────────────────────────────────────────────

void gfx_px(int x, int y, uint16_t color) {
    if (!in_bounds(x, y)) return;
    s_fb[y * GFX_W + x] = color;
}

// Unpack RGB565 → 5/6/5 components, scale to 8-bit-ish for blending math.
static inline void unpack(uint16_t c, int *r, int *g, int *b) {
    *r = (c >> 11) & 0x1F;
    *g = (c >> 5)  & 0x3F;
    *b =  c        & 0x1F;
}

static inline uint16_t pack(int r, int g, int b) {
    return (uint16_t)(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
}

void gfx_px_blend(int x, int y, uint16_t color, uint8_t alpha) {
    if (!in_bounds(x, y)) return;
    if (alpha == 0) return;
    if (alpha == 255) { s_fb[y * GFX_W + x] = color; return; }

    int sr, sg, sb, dr, dg, db;
    unpack(color, &sr, &sg, &sb);
    unpack(s_fb[y * GFX_W + x], &dr, &dg, &db);

    // 0..255 alpha → 0..32 for shift-friendly math
    int a = alpha;
    int ia = 255 - a;
    int r = (sr * a + dr * ia + 127) / 255;
    int g = (sg * a + dg * ia + 127) / 255;
    int b = (sb * a + db * ia + 127) / 255;
    s_fb[y * GFX_W + x] = pack(r, g, b);
}

// ── Bulk ────────────────────────────────────────────────────────────────────

void gfx_clear(uint16_t color) {
    // Word-aligned fast clear
    if (color == 0) {
        memset(s_fb, 0, sizeof(s_fb));
        return;
    }
    for (int i = 0; i < GFX_W * GFX_H; i++) s_fb[i] = color;
}

void gfx_fill_gradient_v(int x, int y, int w, int h, uint16_t top, uint16_t bot) {
    int tr, tg, tb, br, bg, bb;
    unpack(top, &tr, &tg, &tb);
    unpack(bot, &br, &bg, &bb);

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if (yy < 0 || yy >= GFX_H) continue;
        int num = row;
        int den = (h > 1) ? (h - 1) : 1;
        int r = tr + (br - tr) * num / den;
        int g = tg + (bg - tg) * num / den;
        int b = tb + (bb - tb) * num / den;
        uint16_t c = pack(r, g, b);
        int x0 = x < 0 ? 0 : x;
        int x1 = x + w; if (x1 > GFX_W) x1 = GFX_W;
        for (int xx = x0; xx < x1; xx++) s_fb[yy * GFX_W + xx] = c;
    }
}

// ── Lines / rects ───────────────────────────────────────────────────────────

void gfx_hline(int x, int y, int w, uint16_t color) {
    if (y < 0 || y >= GFX_H) return;
    int x0 = x < 0 ? 0 : x;
    int x1 = x + w; if (x1 > GFX_W) x1 = GFX_W;
    for (int xx = x0; xx < x1; xx++) s_fb[y * GFX_W + xx] = color;
}

void gfx_vline(int x, int y, int h, uint16_t color) {
    if (x < 0 || x >= GFX_W) return;
    int y0 = y < 0 ? 0 : y;
    int y1 = y + h; if (y1 > GFX_H) y1 = GFX_H;
    for (int yy = y0; yy < y1; yy++) s_fb[yy * GFX_W + x] = color;
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > GFX_W) x1 = GFX_W;
    int y1 = y + h; if (y1 > GFX_H) y1 = GFX_H;
    for (int yy = y0; yy < y1; yy++) {
        for (int xx = x0; xx < x1; xx++) s_fb[yy * GFX_W + xx] = color;
    }
}

void gfx_rect(int x, int y, int w, int h, uint16_t color) {
    gfx_hline(x,         y,         w, color);
    gfx_hline(x,         y + h - 1, w, color);
    gfx_vline(x,         y,         h, color);
    gfx_vline(x + w - 1, y,         h, color);
}

// ── Rounded rect with AA corners ────────────────────────────────────────────
//
// We compute distance from each corner pixel to the corner arc center.
// alpha = clamp(r + 0.5 - dist, 0, 1) * 255 — classic distance-field AA.

static int isqrt32(uint32_t n) {
    // Integer sqrt for small n
    uint32_t x = n, c = 0, d = 1u << 30;
    while (d > n) d >>= 2;
    while (d != 0) {
        if (x >= c + d) { x -= c + d; c = (c >> 1) + d; }
        else { c >>= 1; }
        d >>= 2;
    }
    return c;
}

// Fixed-point distance with 4-bit fractional part: returns dist << 4
static int dist_q4(int dx, int dy) {
    uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
    // We want sqrt(d2) << 4 = sqrt(d2 << 8)
    return isqrt32(d2 << 8);
}

void gfx_fill_rrect(int x, int y, int w, int h, int r, uint16_t color) {
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 0) r = 0;

    // Body: middle horizontal strip (full width)
    gfx_fill_rect(x, y + r, w, h - 2 * r, color);
    // Top and bottom rectangular bands (between corner arcs)
    gfx_fill_rect(x + r, y,             w - 2 * r, r, color);
    gfx_fill_rect(x + r, y + h - r,     w - 2 * r, r, color);

    if (r == 0) return;

    // Corners with AA
    int r_q4 = (r << 4);
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            // Distance from arc center to *center* of pixel (dx+0.5, dy+0.5)
            int d = dist_q4(2 * dx + 1, 2 * dy + 1) >> 1;  // adjust because we used +1s

            int delta = r_q4 - d;            // positive inside, negative outside
            int alpha;
            if (delta >= 16) alpha = 255;     // ≥1px inside
            else if (delta <= -16) alpha = 0; // ≥1px outside
            else alpha = ((delta + 16) * 255) / 32;  // 1px AA band

            if (alpha == 0) continue;
            // Top-left
            gfx_px_blend(x + r - 1 - dx, y + r - 1 - dy, color, alpha);
            // Top-right
            gfx_px_blend(x + w - r + dx, y + r - 1 - dy, color, alpha);
            // Bottom-left
            gfx_px_blend(x + r - 1 - dx, y + h - r + dy, color, alpha);
            // Bottom-right
            gfx_px_blend(x + w - r + dx, y + h - r + dy, color, alpha);
        }
    }
}

void gfx_rrect(int x, int y, int w, int h, int r, uint16_t color) {
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r < 0) r = 0;

    // Edges between corners
    gfx_hline(x + r, y,             w - 2 * r, color);
    gfx_hline(x + r, y + h - 1,     w - 2 * r, color);
    gfx_vline(x,         y + r,     h - 2 * r, color);
    gfx_vline(x + w - 1, y + r,     h - 2 * r, color);

    if (r == 0) return;

    // AA arcs (1px stroke, ~1px AA band on either side)
    int r_q4 = (r << 4);
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            int d = dist_q4(2 * dx + 1, 2 * dy + 1) >> 1;
            int delta = d - r_q4;            // 0 = on stroke
            int adelta = delta < 0 ? -delta : delta;
            if (adelta > 16) continue;
            int alpha = 255 - (adelta * 255) / 16;

            gfx_px_blend(x + r - 1 - dx, y + r - 1 - dy, color, alpha);
            gfx_px_blend(x + w - r + dx, y + r - 1 - dy, color, alpha);
            gfx_px_blend(x + r - 1 - dx, y + h - r + dy, color, alpha);
            gfx_px_blend(x + w - r + dx, y + h - r + dy, color, alpha);
        }
    }
}

// ── Filled circle (AA) ──────────────────────────────────────────────────────

void gfx_fill_circle(int cx, int cy, int r, uint16_t color) {
    int r_q4 = (r << 4);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d = dist_q4(2 * dx + (dx < 0 ? -1 : 1),
                             2 * dy + (dy < 0 ? -1 : 1)) >> 1;
            int delta = r_q4 - d;
            int alpha;
            if (delta >= 16) alpha = 255;
            else if (delta <= -16) continue;
            else alpha = ((delta + 16) * 255) / 32;
            gfx_px_blend(cx + dx, cy + dy, color, alpha);
        }
    }
}

// ── Glyph / sprite blits ────────────────────────────────────────────────────

void gfx_blit_1bit(int x, int y, const uint8_t *bits, int w, int h,
                   uint16_t color) {
    int stride = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t byte = bits[row * stride + (col >> 3)];
            if (byte & (0x80 >> (col & 7))) {
                gfx_px(x + col, y + row, color);
            }
        }
    }
}

void gfx_blit_rgb565(int x, int y, const uint16_t *src, int w, int h,
                     uint16_t key, bool has_key) {
    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if (yy < 0 || yy >= GFX_H) continue;
        for (int col = 0; col < w; col++) {
            int xx = x + col;
            if (xx < 0 || xx >= GFX_W) continue;
            uint16_t p = src[row * w + col];
            if (has_key && p == key) continue;
            s_fb[yy * GFX_W + xx] = p;
        }
    }
}
