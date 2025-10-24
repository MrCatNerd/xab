#pragma once

#include <xcb/xcb.h>

typedef struct MousePosition {
        float mousex;
        float mousey;
} MousePosition_t;

MousePosition_t get_mouse_position(xcb_connection_t *connection,
                                   xcb_window_t *root);
