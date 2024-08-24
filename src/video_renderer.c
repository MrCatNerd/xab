#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>

#include <epoxy/gl.h>
#include <epoxy/gl_generated.h>
#include <stdlib.h>

#include "video_reader.h"
#include "video_renderer.h"
#include "vertex.h"
#include "utils.h"
#include "logging.h"

// clang-format off
static const Vertex_t vertices[] = {
    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},   // top right
    {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // bottom right
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // bottom left
    {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},  // top left
};

static const unsigned int indices[] = {0, 1, 2, 0, 3, 2};
// clang-format on

double get_time_since_start() {
    static double start_time = 0.0;
    if (start_time == 0.0) {
        // Initialize start_time if it hasn't been set yet
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        start_time = ts.tv_sec + ts.tv_nsec * 1e-9;
    }

    // Get the current time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double current_time = ts.tv_sec + ts.tv_nsec * 1e-9;

    // Return the elapsed time
    return current_time - start_time;
}

Video_t video_from_file(const char *path) {
    Video_t vid;

    // Load the thingy
    if (!video_reader_open(&vid.vr_state, path)) {
        fprintf(stderr, "Couldn't load video frame from: %s\n", path);
        exit(EXIT_FAILURE); // todo: maybe gracefully shut down?
    };

    const int frame_width = vid.vr_state.width;
    const int frame_height = vid.vr_state.height;
    LOG("-- Allocating a %luMB pixel buffer\n",
        ((sizeof(uint8_t) * frame_width * frame_height * 4) / 1048576));
    vid.pbuffer = malloc(sizeof(uint8_t) * frame_width * frame_height * 4);
    Assert(vid.pbuffer != NULL && "Couldn't allocate a pixel buffer");

    // Shader
    // and no im not gonna DRY this code up
    {
        // error stuff
        int success;
        char infoLog[1024];

        // vertex shader
        unsigned int vshader;
        const char *vshader_src = ReadFile("res/shaders/vertex.glsl");
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
        const char *fshader_src = ReadFile("res/shaders/fragment.glsl");
        fshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fshader, 1, &fshader_src, NULL);
        glCompileShader(fshader);

        glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fshader, sizeof(infoLog), NULL, infoLog);
            fprintf(stderr, "Failed to compile fragment shader\n%s\n", infoLog);
        }

        // shader program
        vid.shader_program = glCreateProgram();
        glAttachShader(vid.shader_program, vshader);
        glAttachShader(vid.shader_program, fshader);
        glLinkProgram(vid.shader_program);

        glGetProgramiv(vid.shader_program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(vid.shader_program, sizeof(infoLog), NULL,
                                infoLog);
            fprintf(stderr, "Failed to link shaders!\n%s\n", infoLog);
        }

        // cleanup
        glDeleteShader(vshader);
        glDeleteShader(fshader);

        free((void *)vshader_src);
        free((void *)fshader_src);
    }

    // VBO
    glGenBuffers(1, &vid.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vid.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO
    glGenBuffers(1, &vid.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vid.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    // VAO
    glGenVertexArrays(1, &vid.vao);
    glBindVertexArray(vid.vao);

    // VAO - position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, position));
    glEnableVertexAttribArray(0);
    // VAO - uv
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, uv));

    // VAO - color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, color));
    glEnableVertexAttribArray(2);

    // const unsigned int width = 100, height = 100;
    LOG("-- video width: %d, video height: %d\n", frame_width, frame_width);
    glGenTextures(1, &vid.texture_id);
    glBindTexture(GL_TEXTURE_2D, vid.texture_id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame_width, frame_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, vid.pbuffer);

    // unbind and stuff
    glBindTexture(GL_TEXTURE_2D, 0);

    return vid;
}

void video_render(Video_t *vid, float delta) {
    // read the frame
    static double pt_sec;

    if (pt_sec - get_time_since_start() > 0) {
        return;
    }

    int64_t pts;
    if (!video_reader_read_frame(&vid->vr_state, vid->pbuffer, &pts)) {
        fprintf(stderr, "Couldn't load video frame\n");
        exit(EXIT_FAILURE);
    }

    pt_sec = pts * (double)vid->vr_state.time_base.num /
             (double)vid->vr_state.time_base.den;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid->vr_state.width,
                 vid->vr_state.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 vid->pbuffer);

    // texture stuff
    glTextureParameteri(vid->texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(vid->texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(vid->texture_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(vid->texture_id, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glActiveTexture(
        GL_TEXTURE0); // activate the texture unit first before binding texture
    glBindTexture(GL_TEXTURE_2D, vid->texture_id);

    // shader stuff
    glUseProgram(vid->shader_program);

    // geometry stuff
    glBindVertexArray(vid->vao);
    glBindBuffer(GL_ARRAY_BUFFER, vid->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vid->ebo);

    // finally render
    glDrawElements(GL_TRIANGLES,
                   (unsigned int)(sizeof(indices) / sizeof(*indices)),
                   GL_UNSIGNED_INT, 0);
}

void video_clean(Video_t *vid) {
    video_reader_close(&vid->vr_state);
    free((void *)vid->pbuffer);

    glDeleteVertexArrays(1, &vid->vao);
    glDeleteBuffers(1, &vid->vbo);
    glDeleteBuffers(1, &vid->ebo);
    glDeleteTextures(1, &vid->texture_id);
    glDeleteProgram(vid->shader_program);
}
