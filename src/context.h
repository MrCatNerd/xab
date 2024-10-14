#pragma once

#include <stdbool.h>

#include <xcb/xcb.h>
#include <epoxy/egl.h>

#include "framebuffer.h"
#include "video_renderer.h"

typedef struct context {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        int screen_nbr;

        xcb_window_t *dekstop_window;
        xcb_pixmap_t background_pixmap;

        EGLSurface *surface;
        EGLDisplay display;
        EGLContext *context;

        FrameBuffer_t framebuffer;
        VideoRenderer_t video;
} context_t;

context_t context_create(bool vsync);
