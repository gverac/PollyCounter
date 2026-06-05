// program.h — vtable interface for the selectable programs (Counter, Rep,
// Countdown, Stopwatch, Settings, Menu, ...).

#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <stdbool.h>

#include "state.h"

typedef struct program_s program_t;

// Implemented in main.c — exposed so programs (especially the menu) can request
// transitions without knowing the registry layout.
void app_switch_to(program_id_t id);
void app_open_menu(void);
void app_close_menu(void);
void app_request_sleep(void);
void app_request_flash_mode(void);

struct program_s {
    const char *name;

    void (*on_enter)(void);
    void (*on_exit)(void);

    void (*on_rotate)(int delta);
    void (*on_short_press)(void);     // encoder short press
    void (*on_long_press)(void);      // encoder long press (default: open menu)
    void (*on_double_press)(void);    // encoder double press (default: sleep)

    void (*on_increment)(void);       // + button
    void (*on_decrement)(void);       // - button

    void (*tick)(void);               // periodic, ~every 5 ms
    void (*draw)(void);               // request a full redraw (after wake)
};

#endif
