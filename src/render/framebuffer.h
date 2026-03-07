#pragma once

#include "render/shader.h"
#include "render/shader_cache.h"
#include "render/texture.h"

typedef struct FrameBuffer {
        Texture_t texture;
        unsigned int vbo, vao, ebo;
        Shader_t *shader;

        unsigned int fbo_id;
        unsigned int rbo_id;
} FrameBuffer_t;

FrameBuffer_t create_framebuffer(int width, int height, int gl_internal_format,
                                 ShaderCache_t *scache);

/// use before you start rendering
void render_framebuffer_start_render(FrameBuffer_t *fb);

/// use after you end rendering
void render_framebuffer_end_render(FrameBuffer_t *fb, int dest, float da_time);

/// haha use at your own risk im too tired
void render_framebuffer_borrow_shader(FrameBuffer_t *fb, int dest,
                                      Shader_t *shader);

/// delete the framebuffer
void delete_framebuffer(FrameBuffer_t *fbi, ShaderCache_t *scache);
