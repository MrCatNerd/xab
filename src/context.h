#pragma once

#include "pch.h"

#include "framebuffer.h"
#include "camera.h"
#include "monitor.h"
#include "shader_cache.h"
#include "wallpaper.h"
#include "arg_parser.h"

typedef struct context {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        int screen_nbr;

        xcb_window_t *desktop_window;
        xcb_pixmap_t background_pixmap;

        EGLSurface *surface;
        EGLDisplay display;
        EGLContext *context;

        camera_t camera;

        monitor_t **monitors;
        int monitor_count;

        ShaderCache_t scache; // TODO: use it
        FrameBuffer_t framebuffer;
        wallpaper_t *wallpapers;
        int wallpaper_count;
} context_t;

context_t context_create(struct argument_options *opts);
