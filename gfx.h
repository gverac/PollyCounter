// Framebuffer-based graphics layer for the stitch counter.
// All drawing writes into a 128x160 RGB565 framebuffer in RAM; gfx_present()
// DMAs the whole buffer to the ST7735.

#ifndef GFX_H_
#define GFX_H_

#include <stdint.h>
#include <stdbool.h>

#define GFX_W 128
#define GFX_H 160

// RGB565 palette — whimsical pastel set
#define C_BLACK     0x0000
#define C_WHITE     0xFFFF
#define C_BG        0x18C3   // very dark indigo
#define C_BG_HI     0x2925   // slightly lighter for top gradient
#define C_INK       0xFFFF
#define C_INK_DIM   0xC618
#define C_LABEL     0x9CD3   // soft lilac
#define C_ACCENT    0xFD6A   // peach
#define C_ACCENT_HI 0xFEB0   // lighter peach for gradient top
#define C_MINT      0x6F9D
#define C_MINT_HI   0x8FFE
#define C_PINK      0xFC9F   // bubblegum
#define C_PINK_HI   0xFE5F
#define C_YELLOW    0xFEE0
#define C_LAVENDER  0xB57F
#define C_DIVIDER   0x4208

// Lifecycle
void gfx_init(void);              // sets up DMA, requires display_init() done
void gfx_present(void);           // blit FB to TFT (blocking, DMA-driven)

// Bulk
void gfx_clear(uint16_t color);
void gfx_fill_gradient_v(int x, int y, int w, int h, uint16_t top, uint16_t bottom);

// Pixels
void gfx_px(int x, int y, uint16_t color);
void gfx_px_blend(int x, int y, uint16_t color, uint8_t alpha);  // 0..255

// Lines / rects
void gfx_hline(int x, int y, int w, uint16_t color);
void gfx_vline(int x, int y, int h, uint16_t color);
void gfx_fill_rect(int x, int y, int w, int h, uint16_t color);
void gfx_rect(int x, int y, int w, int h, uint16_t color);

// Rounded rect (filled + stroked variants) with 1px AA on corners
void gfx_fill_rrect(int x, int y, int w, int h, int r, uint16_t color);
void gfx_rrect(int x, int y, int w, int h, int r, uint16_t color);

// Filled circle (small, AA)
void gfx_fill_circle(int cx, int cy, int r, uint16_t color);

// 1-bit glyph bitmap: row-major, MSB-leftmost, padded to whole bytes per row
void gfx_blit_1bit(int x, int y, const uint8_t *bits, int w, int h,
                   uint16_t color);

// RGB565 sprite, optional transparent color key (set to 0 for none)
void gfx_blit_rgb565(int x, int y, const uint16_t *src, int w, int h,
                     uint16_t key, bool has_key);

#endif
