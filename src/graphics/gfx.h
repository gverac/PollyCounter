// Framebuffer-based graphics layer for the stitch counter.
// All drawing writes into a 128x160 RGB565 framebuffer in RAM; gfx_present()
// DMAs the whole buffer to the ST7735.

#ifndef GFX_H_
#define GFX_H_

#include <stdint.h>
#include <stdbool.h>

#include "palette.h"

#define GFX_W 128
#define GFX_H 160

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
