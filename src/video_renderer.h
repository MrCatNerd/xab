#pragma once

#include <stdbool.h>

#include "camera.h"
#include "video_reader.h"

typedef struct VideoRendererConfig {
        bool pixelated;
        /// still not implemented
        bool hw_accel;
} VideoRendererConfig_t;

typedef struct VideoRenderer {
        // public:
        VideoRendererConfig_t config;

        // internal
        unsigned int shader_program;
        unsigned int vbo, vao, ebo;
        unsigned int texture_id;

        unsigned int pbos[2]; // for rendering frame, and one for uploading next
                              // frame

        struct VideoReaderState vr_state;
} VideoRenderer_t;

VideoRenderer_t video_from_file(const char *path, VideoRendererConfig_t config);
void video_render(VideoRenderer_t *vid, int x, int y, int width, int height,
                  camera_t *camera);
void video_clean(VideoRenderer_t *vid);
