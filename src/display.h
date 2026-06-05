#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 160

// RGB565
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_GRAY    0x8410
#define COLOR_DGRAY   0x4208

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
