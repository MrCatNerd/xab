#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include <epoxy/gl.h>
#include <epoxy/gl_generated.h>

#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>

#include "video_reader.h"
#include "video_renderer.h"
#include "vertex.h"
#include "utils.h"
#include "logging.h"
#include "egl_stuff.h"

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

VideoRenderer_t video_from_file(const char *path,
                                VideoRendererConfig_t config) {
    VideoRenderer_t vid;

    // Load the thingy
    if (!video_reader_open(&vid.vr_state, path)) {
        fprintf(stderr, "Couldn't load video frame from: %s\n", path);
        exit(EXIT_FAILURE); // todo: maybe gracefully shut down?
    }

    vid.config = config;

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
        GLCALL(glShaderSource(vshader, 1, &vshader_src, NULL));
        GLCALL(glCompileShader(vshader));

        glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vshader, sizeof(infoLog), NULL, infoLog);
            fprintf(stderr, "Failed to compile vertex shader\n%s\n", infoLog);
        }

        // fragment shader
        unsigned int fshader;
        const char *fshader_src = ReadFile("res/shaders/fragment.glsl");
        fshader = glCreateShader(GL_FRAGMENT_SHADER);
        GLCALL(glShaderSource(fshader, 1, &fshader_src, NULL));
        GLCALL(glCompileShader(fshader));

        glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fshader, sizeof(infoLog), NULL, infoLog);
            fprintf(stderr, "Failed to compile fragment shader\n%s\n", infoLog);
        }

        // shader program
        vid.shader_program = glCreateProgram();
        GLCALL(glAttachShader(vid.shader_program, vshader));
        GLCALL(glAttachShader(vid.shader_program, fshader));
        GLCALL(glLinkProgram(vid.shader_program));

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
    glEnableVertexAttribArray(1);

    // VAO - color
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex_t),
                          (void *)offsetof(Vertex_t, color));
    glEnableVertexAttribArray(2);

    // PBO
    glGenBuffers(2, vid.pbos);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vid.pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, vid.vr_state.frame_size_bytes, 0,
                     GL_STREAM_DRAW);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vid.pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, vid.vr_state.frame_size_bytes, 0,
                     GL_STREAM_DRAW);
    }

    // Texture
    LOG("-- Video dimensions: %dx%dpx\n", vid.vr_state.width,
        vid.vr_state.height);
    glGenTextures(1, &vid.texture_id);
    glBindTexture(GL_TEXTURE_2D, vid.texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, vid.vr_state.width,
                        vid.vr_state.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                        0)); // set all to black

    // unbind stuff
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return vid;
}

void video_render(VideoRenderer_t *vid) {
    // read the frame
    static double pt_sec = 0.0f;

    if (get_time_since_start() - pt_sec < 0) {
        // printf("wait a sec\n");
        // return;
    }

    int64_t pts;
    // upload texture to gpu
    // frame_idx: is used to copy pixels from a PBO to a texture
    // next_frame_idx: is used to update pixels in the other PBO
    const unsigned int frame_idx = 0;
    const unsigned int next_frame_idx = 1;

    //  upload to texture
    glBindTexture(GL_TEXTURE_2D, vid->texture_id);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vid->pbos[frame_idx]);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vid->vr_state.width,
                    vid->vr_state.height, GL_RGB, GL_UNSIGNED_BYTE, 0);

    //  get current frame
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vid->pbos[next_frame_idx]);

    // Note that glMapBuffer() causes sync issue. If GPU is working with
    // this buffer, glMapBuffer() will wait until GPU to finish its job. To
    // avoid waiting, you can call first glBufferData() with NULL pointer
    // before glMapBuffer(). If you do that, the previous data in PBO will
    // be discarded and glMapBuffer() returns a new allocated pointer
    // immediately even if GPU is still working with the previous data.
    GLCALL(glBufferData(GL_PIXEL_UNPACK_BUFFER, vid->vr_state.frame_size_bytes,
                        0, GL_STREAM_DRAW));
    GLubyte *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) {
        if (!video_reader_read_frame(&vid->vr_state, ptr, &pts)) {
            fprintf(stderr, "Couldn't load video frame\n");
            exit(EXIT_FAILURE);
        }

        pt_sec = pts * av_q2d(vid->vr_state.time_base);

        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    // swap the pixel buffers (with the id)
    const unsigned int temp = vid->pbos[next_frame_idx];
    vid->pbos[next_frame_idx] = vid->pbos[frame_idx];
    vid->pbos[frame_idx] = temp;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind when done

    // texture stuff
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    vid->config.pixelated ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    vid->config.pixelated ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glActiveTexture(GL_TEXTURE0);

    // shader stuff
    GLCALL(glUseProgram(vid->shader_program));
    glUniform1f(glGetUniformLocation(vid->shader_program, "Time"),
                get_time_since_start());

    // geometry stuff
    glBindVertexArray(vid->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vid->ebo);

    // finally render
    GLCALL(glDrawElements(GL_TRIANGLES,
                          (unsigned int)(sizeof(indices) / sizeof(*indices)),
                          GL_UNSIGNED_INT, 0));

    // unbind stuff
    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void video_clean(VideoRenderer_t *vid) {
    video_reader_close(&vid->vr_state);

    glDeleteBuffers(1, &vid->ebo);
    glDeleteBuffers(1, &vid->vbo);
    glDeleteBuffers(2, vid->pbos);
    glDeleteTextures(1, &vid->texture_id);
    glDeleteProgram(vid->shader_program);
    glDeleteVertexArrays(1, &vid->vao);
}
