#pragma once

#include "pch.h"

#include "framebuffer.h"
#include "camera.h"
#include "monitor.h"
#include "shader_cache.h"
#include "wallpaper.h"
#include "arg_parser.h"
#include "window.h"
#include "x_data.h"

typedef struct context {
        x_data_t xdata;

        EGLDisplay display;
        Window_t window;

        Camera_t camera;

        monitor_t **monitors;
        int monitor_count;

        ShaderCache_t scache;
        FrameBuffer_t framebuffer;
        wallpaper_t *wallpapers;
        int wallpaper_count;
} context_t;

context_t context_create(struct argument_options *opts);
void context_free(context_t *context);
