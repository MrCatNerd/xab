#pragma once

#include <stdbool.h>

#include <xcb/xcb.h>
#include <EGL/egl.h>

#include "framebuffer.h"
#include "video_renderer.h"

typedef struct context {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        int screen_nbr;

        // even though they are the same i still wanna get both because I CAN
        xcb_pixmap_t background_pixmap;
        xcb_pixmap_t *xroot_window;
        xcb_pixmap_t *esetroot_window;

        EGLSurface *surface;
        EGLDisplay display;
        EGLContext *context;

        FrameBuffer_t framebuffer;
        VideoRenderer_t video;
} context_t;

context_t context_create(bool vsync);
