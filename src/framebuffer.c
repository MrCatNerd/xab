#include <stdio.h>
#include <stdlib.h>

#include <epoxy/common.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <epoxy/gl_generated.h>
#include <epoxy/egl_generated.h>

#include "framebuffer.h"
#include "logging.h"
#include "utils.h"
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

FrameBuffer_t create_framebuffer(int width, int height) {
    // create fbo
    FrameBuffer_t fb;
    glGenFramebuffers(1, &fb.fbo_id);

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo_id);

    // create rbo
    glGenRenderbuffers(1, &fb.rbo_id);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo_id);

    fb.width = width;
    fb.height = height;

    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fb.width,
                          fb.height);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, fb.rbo_id);

    // create texture
    glGenTextures(1, &fb.texture_color_id);

    if (fb.texture_color_id == 0) {
        program_error("Failed to generate framebuffer color texture!\n");
    }
    glBindTexture(GL_TEXTURE_2D, fb.texture_color_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb.width, fb.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fb.texture_color_id, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        LOG("-- Created framebuffer #%d %dx%dpx\n", fb.fbo_id, fb.width,
            fb.height);
    } else
        program_error("framebuffer #%d not complete\n", fb.fbo_id);

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

    // we don't need color data

    // Shader
    // totally not copied from video_renderer.c
    {
        // error stuff
        int success;
        char infoLog[1024];

        // vertex shader
        unsigned int vshader;
        const char *vshader_src =
            ReadFile("res/shaders/framebuffer_vertex.glsl");
        vshader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vshader, 1, &vshader_src, NULL);
        glCompileShader(vshader);

        glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vshader, sizeof(infoLog), NULL, infoLog);
            fprintf(stderr, "Failed to compile vertex shader\n%s\n", infoLog);
        }

        // fragment shader
        unsigned int fshader;
        const char *fshader_src =
            ReadFile("res/shaders/framebuffer_fragment.glsl");
        fshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fshader, 1, &fshader_src, NULL);
        glCompileShader(fshader);

        glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fshader, sizeof(infoLog), NULL, infoLog);
            fprintf(stderr, "Failed to compile fragment shader\n%s\n", infoLog);
        }

        // shader program
        fb.shader_program = glCreateProgram();
        glAttachShader(fb.shader_program, vshader);
        glAttachShader(fb.shader_program, fshader);
        glLinkProgram(fb.shader_program);

        glGetProgramiv(fb.shader_program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(fb.shader_program, sizeof(infoLog), NULL,
                                infoLog);
            fprintf(stderr, "Failed to link shaders!\n%s\n", infoLog);
        }

        // cleanup
        glDeleteShader(vshader);
        glDeleteShader(fshader);

        free((void *)vshader_src);
        free((void *)fshader_src);
    }

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
    // first pass
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo_id);

    glClearColor(0.8f, 0.6f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void render_framebuffer_end_render(FrameBuffer_t *fb) {
    // second pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    // don't clear anything cuz we wanna preserve the other backgrounds
    // glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb->texture_color_id);
    glUseProgram(fb->shader_program);
    glBindVertexArray(fb->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fb->ebo);
    glDrawElements(GL_TRIANGLES,
                   (unsigned int)(sizeof(indices) / sizeof(*indices)),
                   GL_UNSIGNED_INT, 0);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void delete_framebuffer(FrameBuffer_t *fb) {
    glDeleteBuffers(1, &fb->ebo);
    glDeleteBuffers(1, &fb->vbo);
    glDeleteFramebuffers(1, &fb->fbo_id);
    glDeleteProgram(fb->shader_program);
    glDeleteRenderbuffers(GL_RENDERBUFFER, &fb->rbo_id);
    glDeleteTextures(1, &fb->texture_color_id);
    glDeleteVertexArrays(1, &fb->vao);
}
