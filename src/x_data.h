#pragma once

#include "pch.h"

#include <xcb/xcb.h>

typedef struct x_data {
        xcb_connection_t *connection;
        xcb_visualtype_t *visual;
        xcb_screen_t *screen;
        int screen_nbr;
} x_data_t;

x_data_t x_data_from_xcb_connection(xcb_connection_t *connection,
                                    int screen_nbr);
