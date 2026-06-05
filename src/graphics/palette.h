// palette.h — single source of truth for all named RGB565 colors.
//
// All UI code reads colors from here. Grouped by intent so future palette
// tweaks only need to touch this file.

#ifndef PALETTE_H_
#define PALETTE_H_

// ── Primitives ─────────────────────────────────────────────────────────────
#define C_BLACK     0x0000
#define C_WHITE     0xFFFF

// ── UI neutrals (used by gfx + program screens) ────────────────────────────
#define C_BG        0x18C3   // very dark indigo
#define C_BG_HI     0x2925   // slightly lighter for top gradient
#define C_INK       0xFFFF
#define C_INK_DIM   0xC618
#define C_LABEL     0x9CD3   // soft lilac — for secondary text
#define C_DIVIDER   0x4208

// ── Pastel palette (whimsical / tamagotchi) ───────────────────────────────
#define C_ACCENT    0xFD6A   // peach — primary selection / highlight
#define C_ACCENT_HI 0xFEB0   // lighter peach for gradient top
#define C_MINT      0x6F9D
#define C_MINT_HI   0x8FFE
#define C_PINK      0xFC9F   // bubblegum
#define C_PINK_HI   0xFE5F
#define C_YELLOW    0xFEE0
#define C_LAVENDER  0xB57F

// ── Counter "celebration" / hit screen ─────────────────────────────────────
#define C_HIT_BG       0xFCD0   // deeper, more saturated pink
#define C_HIT_INK      0x2086   // very dark navy/plum
#define C_HIT_INK_DIM  0x73AE   // greyer ink for label/goal on hit screen
#define C_HIT_BAR_DIM  0xFBB6   // dim part of progress bar on hit screen
#define C_SEGMENT_DIM  0x18C3   // dim progress segment (non-hit)
#define C_SEGMENT_LIT  0xFFFF   // lit progress segment

// ── Legacy COLOR_* aliases used by display_show_message() ─────────────────
// Programs should prefer the C_* names above. Kept for the public message API.
#define COLOR_BLACK   C_BLACK
#define COLOR_WHITE   C_WHITE
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  C_YELLOW
#define COLOR_CYAN    0x07FF
#define COLOR_GRAY    0x8410
#define COLOR_DGRAY   0x4208

#endif
