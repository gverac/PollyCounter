#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#include "palette.h"

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 160

void display_init(void);
void display_clear(uint16_t color);

// Blit a full-screen RGB565 framebuffer using DMA. Blocking — returns when done.
void display_blit_full(const uint16_t *fb);

void display_draw_screen(uint32_t count, uint32_t target);
void display_update_count(uint32_t count, uint32_t target);
void display_update_target(uint32_t count, uint32_t target);
void display_show_message(const char *line1, const char *line2, uint16_t color);

// Generic text helpers (write into the gfx framebuffer; caller does gfx_present).
void display_text(const char *s, int x, int y, int scale, uint16_t color);
int  display_text_width(const char *s, int scale);

// Drive the celebration animation. Call from the main loop. No-op when idle.
void display_tick(void);

#endif
