#pragma once

#include "shader.h"

typedef struct FrameBuffer {
        unsigned int width, height;
        unsigned int vbo, vao, ebo;
        Shader_t shader;

        unsigned int fbo_id;
        unsigned int rbo_id;
        unsigned int texture_color_id;
        int gl_internal_format;
} FrameBuffer_t;

FrameBuffer_t create_framebuffer(int width, int height, int gl_internal_format);

/// use before you start rendering
void render_framebuffer_start_render(FrameBuffer_t *fb);

/// use after you end rendering
void render_framebuffer_end_render(FrameBuffer_t *fb, int dest, float da_time);

/// haha use at your own risk im too tired
void render_framebuffer_borrow_shader(FrameBuffer_t *fb, int dest,
                                      Shader_t *shader);

/// delete the framebuffer
void delete_framebuffer(FrameBuffer_t *fb);
