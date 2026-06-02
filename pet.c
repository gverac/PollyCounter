#include "pet.h"

#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "display.h"
#include "gfx.h"

// ── Colors ──────────────────────────────────────────────────────────────────

#define PET_BG       0x6BBE   // soft twilight lavender — high contrast with face
#define PET_GRASS    0x2D88   // brighter grass green
#define WOOL         0xFFFF   // sheep wool
#define WOOL_SHADE   0xC618   // soft grey for shading
#define FACE         0x0000   // pure black face/legs — sharp contrast
#define EYE          0xFFFF
#define HEART        0xF81F   // magenta heart
#define HEART_DIM    0x4008
#define CRUMB        0x9381   // tan/grass
#define TIMEOUT_PROG 0x4208   // bar background
#define TIMEOUT_FILL 0xFCD0   // pink

// ── Timing ──────────────────────────────────────────────────────────────────

#define INACTIVITY_MS    5000
#define FRAME_MS         55
#define HAPPY_MS         900
#define EATING_MS        1300
#define JUMP_MS          700
#define MAX_HAPPINESS    5

// ── State ───────────────────────────────────────────────────────────────────

typedef enum {
    SHEEP_IDLE,
    SHEEP_HAPPY,
    SHEEP_EATING,
    SHEEP_JUMP,
} sheep_state_t;

#define PARTICLE_MAX 16
typedef struct {
    int16_t x, y;     // 8.8 fixed-point position
    int16_t vx, vy;   // 8.8 fixed-point velocity
    uint8_t life;     // frames remaining
    uint8_t kind;     // 0 = heart, 1 = crumb
} pet_particle_t;

static bool            s_active           = false;
static sheep_state_t   s_state            = SHEEP_IDLE;
static uint32_t        s_state_start_ms   = 0;
static uint32_t        s_last_input_ms    = 0;
static uint32_t        s_last_frame_ms    = 0;
static uint32_t        s_frame            = 0;

static int             s_sheep_x          = 64;   // body center
static int             s_target_x         = 64;
static int             s_happiness        = 0;

static pet_particle_t  s_particles[PARTICLE_MAX];

// Tiny xorshift PRNG, seeded on entry
static uint32_t s_rng = 0xBEEF1234u;
static inline uint32_t rng_next(void) {
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

// ── Particles ──────────────────────────────────────────────────────────────

static void spawn_particle(int x, int y, int16_t vx, int16_t vy,
                            uint8_t life, uint8_t kind) {
    for (int i = 0; i < PARTICLE_MAX; i++) {
        if (s_particles[i].life != 0) continue;
        s_particles[i].x    = x << 8;
        s_particles[i].y    = y << 8;
        s_particles[i].vx   = vx;
        s_particles[i].vy   = vy;
        s_particles[i].life = life;
        s_particles[i].kind = kind;
        return;
    }
}

static void update_particles(void) {
    for (int i = 0; i < PARTICLE_MAX; i++) {
        if (s_particles[i].life == 0) continue;
        s_particles[i].x  += s_particles[i].vx;
        s_particles[i].y  += s_particles[i].vy;
        if (s_particles[i].kind == 1) {
            // Crumbs fall with gravity
            s_particles[i].vy += 18;
        }
        s_particles[i].life--;
    }
}

static void draw_heart(int x, int y, uint16_t color) {
    // 5x5 pixel heart
    gfx_px(x + 1, y,     color);
    gfx_px(x + 3, y,     color);
    gfx_fill_rect(x, y + 1, 5, 1, color);
    gfx_fill_rect(x, y + 2, 5, 1, color);
    gfx_fill_rect(x + 1, y + 3, 3, 1, color);
    gfx_px(x + 2, y + 4, color);
}

static void draw_particles(void) {
    for (int i = 0; i < PARTICLE_MAX; i++) {
        if (s_particles[i].life == 0) continue;
        int px = s_particles[i].x >> 8;
        int py = s_particles[i].y >> 8;
        if (s_particles[i].kind == 0) {
            // Heart: full 5x5 while alive, fade to single pixel near end
            uint16_t c = (s_particles[i].life > 6) ? HEART : HEART_DIM;
            if (s_particles[i].life > 3) draw_heart(px, py, c);
            else                          gfx_px(px + 2, py + 2, c);
        } else {
            // Grass crumb: 2x2
            gfx_fill_rect(px, py, 2, 2, CRUMB);
        }
    }
}

// ── Sheep drawing ──────────────────────────────────────────────────────────

static void draw_sheep(int cx, int cy, sheep_state_t state, uint32_t frame) {
    // Breathing: small vertical bounce
    int breath = ((frame >> 3) & 1) ? 0 : -1;
    cy += breath;

    // EATING: head down + body slightly forward
    int head_dy = 0;
    if (state == SHEEP_EATING) head_dy = 6;

    // Body — overlapping circles for fluff
    // (JUMP doesn't change the sheep itself; just its y position, applied by caller)
    gfx_fill_circle(cx - 11, cy + 2, 6, WOOL);
    gfx_fill_circle(cx - 4,  cy,     7, WOOL);
    gfx_fill_circle(cx + 4,  cy,     7, WOOL);
    gfx_fill_circle(cx + 11, cy + 2, 6, WOOL);
    gfx_fill_circle(cx - 4,  cy + 4, 6, WOOL);
    gfx_fill_circle(cx + 4,  cy + 4, 6, WOOL);

    // Subtle bottom shading line
    gfx_fill_rect(cx - 14, cy + 7, 28, 1, WOOL_SHADE);

    // Legs (4 short ones) — start below the wool so they don't get covered,
    // 3px wide for visibility, 6px tall so they extend clearly past the body.
    int leg_y = cy + 8;
    gfx_fill_rect(cx - 11, leg_y, 3, 6, FACE);
    gfx_fill_rect(cx - 5,  leg_y, 3, 6, FACE);
    gfx_fill_rect(cx + 2,  leg_y, 3, 6, FACE);
    gfx_fill_rect(cx + 8,  leg_y, 3, 6, FACE);

    // Head — slightly bigger, on the right side, with a wool tuft on top
    int hx = cx + 14;
    int hy = cy - 2 + head_dy;
    gfx_fill_circle(hx, hy, 6, FACE);
    // Fluffy forelock — a small white wool tuft on the head for extra cuteness
    gfx_fill_circle(hx - 3, hy - 5, 3, WOOL);
    // Ears (little tufts on the sides of the head)
    gfx_fill_rect(hx - 5, hy - 4, 2, 3, FACE);
    gfx_fill_rect(hx + 3, hy - 4, 2, 3, FACE);

    // Eyes (blink occasionally in idle)
    bool blink = (state == SHEEP_IDLE) && ((frame % 80) < 4);
    if (state == SHEEP_EATING) {
        // Closed-eye munching arcs
        gfx_fill_rect(hx - 3, hy - 1, 2, 1, EYE);
        gfx_fill_rect(hx + 1, hy - 1, 2, 1, EYE);
    } else if (!blink) {
        gfx_px(hx - 2, hy - 1, EYE);
        gfx_px(hx + 2, hy - 1, EYE);
        // Tiny pupils for personality
        gfx_px(hx - 2, hy,     EYE);
        gfx_px(hx + 2, hy,     EYE);
    }

    // Happy mouth (smile) when HAPPY
    if (state == SHEEP_HAPPY) {
        gfx_px(hx - 1, hy + 2, EYE);
        gfx_px(hx,     hy + 3, EYE);
        gfx_px(hx + 1, hy + 2, EYE);
    }
}

// ── Happiness meter ─────────────────────────────────────────────────────────

static void draw_happiness(void) {
    // Row of hearts at top
    int n = MAX_HAPPINESS;
    int spacing = 8;
    int total = n * spacing - 3;
    int x = (DISPLAY_WIDTH - total) / 2;
    for (int i = 0; i < n; i++) {
        uint16_t color = (i < s_happiness) ? HEART : HEART_DIM;
        draw_heart(x + i * spacing, 8, color);
    }
}

// ── Timeout indicator ───────────────────────────────────────────────────────

static void draw_timeout_bar(void) {
    uint32_t since = to_ms_since_boot(get_absolute_time()) - s_last_input_ms;
    if (since >= INACTIVITY_MS) since = INACTIVITY_MS;
    // Bar shrinks as inactivity grows — visual countdown
    int total_w = 60;
    int x = (DISPLAY_WIDTH - total_w) / 2;
    int y = 150;
    int filled = total_w - (int)((since * total_w) / INACTIVITY_MS);
    gfx_fill_rect(x, y, total_w, 3, TIMEOUT_PROG);
    if (filled > 0) gfx_fill_rect(x, y, filled, 3, TIMEOUT_FILL);
}

// ── Scene ──────────────────────────────────────────────────────────────────

static void draw_scene(void) {
    gfx_clear(PET_BG);

    // Grass strip at bottom (sheep stands on top of it)
    gfx_fill_rect(0, 100, DISPLAY_WIDTH, 24, PET_GRASS);

    draw_happiness();

    // Sheep walks toward target_x
    if (s_sheep_x < s_target_x) s_sheep_x++;
    else if (s_sheep_x > s_target_x) s_sheep_x--;
    if (s_sheep_x < 20) s_sheep_x = 20;
    if (s_sheep_x > DISPLAY_WIDTH - 25) s_sheep_x = DISPLAY_WIDTH - 25;

    // HAPPY state: small bounce
    int sheep_y = 100;
    if (s_state == SHEEP_HAPPY) {
        int phase = (s_frame >> 1) & 7;
        int bounce[8] = {0, -2, -4, -3, -2, -1, 0, 0};
        sheep_y += bounce[phase];
    } else if (s_state == SHEEP_JUMP) {
        // Smooth parabolic hop. y = peak - peak * (dt/half)^2, dt = elapsed - half.
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - s_state_start_ms;
        if (elapsed > JUMP_MS) elapsed = JUMP_MS;
        int half = JUMP_MS / 2;
        int dt   = (int)elapsed - half;
        int peak = 24;                         // jump height in pixels
        int y_off = peak - (peak * dt * dt) / (half * half);
        if (y_off < 0) y_off = 0;
        sheep_y -= y_off;
    }

    draw_sheep(s_sheep_x, sheep_y, s_state, s_frame);
    draw_particles();
    draw_timeout_bar();
}

// ── State transitions ──────────────────────────────────────────────────────

static void enter_state(sheep_state_t s) {
    s_state          = s;
    s_state_start_ms = to_ms_since_boot(get_absolute_time());
}

static void mark_input(void) {
    s_last_input_ms = to_ms_since_boot(get_absolute_time());
}

static void bump_happiness(void) {
    if (s_happiness < MAX_HAPPINESS) s_happiness++;
}

// ── Public API ─────────────────────────────────────────────────────────────

void pet_enter(void) {
    s_active         = true;
    s_state          = SHEEP_IDLE;
    s_state_start_ms = to_ms_since_boot(get_absolute_time());
    s_last_input_ms  = s_state_start_ms;
    s_last_frame_ms  = 0;
    s_frame          = 0;
    s_sheep_x        = 64;
    s_target_x       = 64;
    s_happiness      = 0;
    s_rng           ^= s_state_start_ms;
    memset(s_particles, 0, sizeof(s_particles));
    draw_scene();
    gfx_present();
}

bool pet_is_active(void) { return s_active; }

void pet_tick(void) {
    if (!s_active) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Auto-exit on inactivity
    if (now - s_last_input_ms >= INACTIVITY_MS) {
        s_active = false;
        return;
    }

    // Frame pacing
    if (now - s_last_frame_ms < FRAME_MS) return;
    s_last_frame_ms = now;
    s_frame++;

    // Auto-revert to idle after state duration
    uint32_t elapsed = now - s_state_start_ms;
    if      (s_state == SHEEP_HAPPY  && elapsed > HAPPY_MS)  enter_state(SHEEP_IDLE);
    else if (s_state == SHEEP_EATING && elapsed > EATING_MS) enter_state(SHEEP_IDLE);
    else if (s_state == SHEEP_JUMP   && elapsed > JUMP_MS)   enter_state(SHEEP_IDLE);

    update_particles();
    draw_scene();
    gfx_present();
}

void pet_on_scratch(void) {
    if (!s_active) return;
    mark_input();
    bump_happiness();
    enter_state(SHEEP_HAPPY);
    // Pop a few hearts above the sheep
    for (int i = 0; i < 3; i++) {
        int dx = (int)(rng_next() % 16) - 8;
        int16_t vx = (int16_t)((rng_next() & 0x7F) - 64);
        int16_t vy = -180 - (int16_t)(rng_next() & 0x3F);
        spawn_particle(s_sheep_x + dx, 90, vx, vy, 22, 0);
    }
}

void pet_on_feed(void) {
    if (!s_active) return;
    mark_input();
    bump_happiness();
    enter_state(SHEEP_EATING);
    // Scatter a few grass crumbs around the head
    for (int i = 0; i < 6; i++) {
        int dx = (int)(rng_next() % 14) - 7;
        int16_t vx = (int16_t)((rng_next() & 0xFF) - 128);
        int16_t vy = -120 - (int16_t)(rng_next() & 0x3F);
        spawn_particle(s_sheep_x + 14 + dx, 108, vx, vy, 18, 1);
    }
}

void pet_on_rotate(int delta) {
    if (!s_active) return;
    mark_input();
    s_target_x += delta * 6;
    if (s_target_x < 20) s_target_x = 20;
    if (s_target_x > DISPLAY_WIDTH - 25) s_target_x = DISPLAY_WIDTH - 25;
}

void pet_on_trick(void) {
    if (!s_active) return;
    mark_input();
    bump_happiness();
    enter_state(SHEEP_JUMP);
}
