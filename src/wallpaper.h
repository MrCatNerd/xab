#pragma once

#include "camera.h"
#include "shader.h"
#include "framebuffer.h"
#include "shader_cache.h"
#include "video_reader_interface.h"

// for the arg parser, not the init
typedef struct wallpaper_options {
        const char *path;
        int width, height;
        int x, y;
} wallpaper_options_t;

typedef struct wallpaper {
        int x, y;
        VideoReaderState_t video;

        Shader_t *shader;
} wallpaper_t;

void wallpaper_init(float scale, int width, int height, int x, int y,
                    bool pixelated, const char *video_path, wallpaper_t *dest,
                    int hw_accel, ShaderCache_t *scache);

void wallpaper_render(wallpaper_t *wallpaper, Camera_t *camera,
                      FrameBuffer_t *fbo_dest);

void wallpaper_close(wallpaper_t *wallpaper, ShaderCache_t *scache);
