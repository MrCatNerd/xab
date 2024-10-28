#pragma once

#include "video_renderer.h"

typedef struct wallpaper_options {
        const char *path;
        int width, height;
        int x, y;
} wallpaper_options_t;

typedef struct wallpaper {
        VideoRenderer_t video;

        // will be ignored without cglm
        int width, height;
        float x, y;
} wallpaper_t;

void wallpaper_init(int width, int height, int x, int y, const char *video_path,
                    VideoRendererConfig_t video_config, wallpaper_t *dest);

void wallpaper_render(wallpaper_t *wallpaper, camera_t *camera);
