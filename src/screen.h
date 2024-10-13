#pragma once

#include <xcb/xproto.h>

typedef struct screen {
        xcb_screen_t *xscreen;
        int screen_nbr;
} screen_t;
