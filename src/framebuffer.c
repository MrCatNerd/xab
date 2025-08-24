#include "pch.h"

#include "framebuffer.h"
#include "logger.h"
#include "shader.h"
#include "shader_cache.h"
#include "vertex.h"

// clang-format off
static const Vertex_t vertices[] = {
    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},   // top right
    {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // bottom right
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // bottom left
    {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},  // top left
};

static const unsigned int indices[] = {0, 1, 2, 0, 3, 2};
// clang-format on

FrameBuffer_t create_framebuffer(int width, int height, int gl_internal_format,
                                 ShaderCache_t *scache) {
    // create fbo
    FrameBuffer_t fb;
    glGenFramebuffers(1, &fb.fbo_id);

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo_id);

    // create rbo
    glGenRenderbuffers(1, &fb.rbo_id);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo_id);

    fb.width = width;
    fb.height = height;
    fb.gl_internal_format = gl_internal_format;

    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fb.width,
                          fb.height);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, fb.rbo_id);

    // create texture
    glGenTextures(1, &fb.texture_color_id);

    if (fb.texture_color_id == 0) {
        xab_log(LOG_ERROR, "Failed to generate framebuffer color texture!\n");
        exit(EXIT_FAILURE);
    }

    glBindTexture(GL_TEXTURE_2D, fb.texture_color_id);
    glTexImage2D(GL_TEXTURE_2D, 0, fb.gl_internal_format, fb.width, fb.height,
                 0, fb.gl_internal_format, GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

    // attach texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fb.texture_color_id, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        xab_log(LOG_ERROR, "framebuffer #%d not complete\n", fb.fbo_id);
        exit(EXIT_FAILURE);
    }

    xab_log(LOG_DEBUG, "Created framebuffer #%d %dx%dpx\n", fb.fbo_id, fb.width,
            fb.height);

    // create quad
    // VBO
    glGenBuffers(1, &fb.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, fb.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO
    glGenBuffers(1, &fb.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fb.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    // VAO
    glGenVertexArrays(1, &fb.vao);
    glBindVertexArray(fb.vao);

    // VAO - position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, position));
    glEnableVertexAttribArray(0);

    // VAO - uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, uv));
    glEnableVertexAttribArray(1);

    // i don't want color data, or do i?
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, color));
    glEnableVertexAttribArray(2);

    fb.shader = shader_cache_create_or_cache_shader(
        "res/shaders/framebuffer_vertex.glsl",
        "res/shaders/framebuffer_fragment.glsl", scache);
    // "res/shaders/mouse_distance_thingy.glsl");

    // unbind buffers
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return fb;
}

void render_framebuffer_start_render(FrameBuffer_t *fb) {
    TracyCZoneNC(tracy_ctx, "FB_START_RENDER", TRACY_COLOR_RED, true);

    // first pass
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo_id);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // oopsie square vertices are (sorta) wrong and im
                             // too lazy to fix them because it doesn't matter

    TracyCZoneEnd(tracy_ctx);
}

void render_framebuffer_end_render(FrameBuffer_t *fb, int dest, float da_time) {
    TracyCZoneNC(tracy_ctx, "FB_END_RENDER", TRACY_COLOR_RED, true);

    // second pass
    glBindFramebuffer(GL_FRAMEBUFFER, dest);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // shader stuff
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb->texture_color_id);
    use_shader(fb->shader);
    glUniform1f(shader_get_uniform_location(fb->shader, "u_Time"), da_time);

    // geometry stuff
    glBindVertexArray(fb->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fb->ebo);

    // finally render
    glDrawElements(GL_TRIANGLES,
                   (unsigned int)(sizeof(indices) / sizeof(*indices)),
                   GL_UNSIGNED_INT, 0);

    // unbind stuff
    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    TracyCZoneEnd(tracy_ctx);
}

void render_framebuffer_borrow_shader(FrameBuffer_t *fb, int dest,
                                      Shader_t *shader) {
    TracyCZoneNC(tracy_ctx, "FB_BORROW_RENDER", TRACY_COLOR_RED, true);

    // second pass
    glBindFramebuffer(GL_FRAMEBUFFER, dest);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    // don't clear anything cuz we wanna preserve the other backgrounds

    // use the shader
    use_shader(shader);

    // geometry stuff
    glBindVertexArray(fb->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fb->ebo);

    // finally render
    glDrawElements(GL_TRIANGLES,
                   (unsigned int)(sizeof(indices) / sizeof(*indices)),
                   GL_UNSIGNED_INT, 0);

    // unbind stuff
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    TracyCZoneEnd(tracy_ctx);
}

void delete_framebuffer(FrameBuffer_t *fb, ShaderCache_t *scache) {
    // delete stuff
    xab_log(LOG_VERBOSE, "Deleting framebuffer #%d\n", fb->fbo_id);
    shader_cache_unref_shader(fb->shader, scache);
    glDeleteTextures(1, &fb->texture_color_id);
    glDeleteBuffers(1, &fb->ebo);
    glDeleteBuffers(1, &fb->vbo);
    glDeleteVertexArrays(1, &fb->vao);
    glDeleteRenderbuffers(1, &fb->rbo_id);
    glDeleteFramebuffers(1, &fb->fbo_id);
}
