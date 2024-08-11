#pragma once

#include <xcb/xcb.h>
#include <EGL/egl.h>

typedef struct {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        int screen_nbr;
        xcb_window_t window;

        // even though they are the same i still wanna get both because I CAN
        xcb_window_t *xroot_window;
        xcb_window_t *esetroot_window;

        EGLSurface *surface;
        EGLDisplay display;
        EGLContext *context;
} Video;
