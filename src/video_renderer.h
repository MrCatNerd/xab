#pragma once

#include <stdbool.h>

#include "video_reader.h"

typedef struct VideoRenderer {
        // internal
        unsigned int shader_program;
        unsigned int vbo, vao, ebo;
        unsigned int texture_id;
        bool pixelated;

        struct VideoReaderState vr_state;
        uint8_t *pbuffer;
} VideoRenderer_t;

VideoRenderer_t video_from_file(const char *path, bool pixelated);
void video_render(VideoRenderer_t *vid);
void video_clean(VideoRenderer_t *vid);
