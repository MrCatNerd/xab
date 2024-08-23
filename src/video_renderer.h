#pragma once

#include <stdbool.h>
#include "video_reader.h"

typedef struct {
        // internal
        unsigned int shader_program;
        unsigned int vbo, vao, ebo;
        unsigned int texture_id;

        struct VideoReaderState vr_state;
        uint8_t *pbuffer;
} Video_t;

Video_t video_from_file(const char *path);
void video_render(Video_t *vid, float delta);
void video_clean(Video_t *vid);
