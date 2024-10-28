#include <stdio.h>

#include "wallpaper.h"
#include "camera.h"
#include "video_renderer.h"

void wallpaper_init(int width, int height, int x, int y, const char *video_path,
                    VideoRendererConfig_t video_config, wallpaper_t *dest) {
    dest->width = width;
    dest->height = height;
    dest->x = x;
    dest->y = y;
    dest->video = video_from_file(video_path, video_config);
}

void wallpaper_render(wallpaper_t *wallpaper, camera_t *camera) {
#ifdef HAVE_LIBCGLM
    video_render(&wallpaper->video, wallpaper->x, wallpaper->y,
                 wallpaper->width, wallpaper->height, camera);
#else
    video_render(&wallpaper->video, wallpaper->x, wallpaper->y,
                 wallpaper->width, wallpaper->height, camera);
#endif
}
