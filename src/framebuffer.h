#pragma once

typedef struct FrameBuffer {
        unsigned int width, height;
        unsigned int vbo, vao, ebo;
        unsigned int shader_program;

        unsigned int fbo_id;
        unsigned int rbo_id;
        unsigned int texture_color_id;
} FrameBuffer_t;

FrameBuffer_t create_framebuffer(int width, int height);

/// use before you start rendering
void render_framebuffer_start_render(FrameBuffer_t *fb);

/// use after you end rendering
void render_framebuffer_end_render(FrameBuffer_t *fb, float da_time);

void delete_framebuffer(FrameBuffer_t *fb);
