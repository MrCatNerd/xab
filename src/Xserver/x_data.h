#pragma once

#include <xcb/xcb.h>

typedef struct x_data {
        xcb_connection_t *connection;
        xcb_visualtype_t *visual;
        xcb_screen_t *screen;
        int screen_nbr;
} x_data_t;

x_data_t x_data_init(void);
void x_data_free(x_data_t *xdata);
