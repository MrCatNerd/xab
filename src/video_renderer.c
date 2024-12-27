#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include <epoxy/gl.h>

#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>

#include "camera.h"
#include "video_reader.h"
#include "video_renderer.h"
#include "vertex.h"
#include "utils.h"
#include "logger.h"
#include "egl_stuff.h"

#ifdef HAVE_LIBCGLM
#include <cglm/types.h>
#include <cglm/struct.h>
#include <cglm/io.h>
#include <cglm/mat4.h>
#include <cglm/affine.h>
#include <cglm/cam.h>
#endif

// clang-format off
static const Vertex_t vertices[] = {
    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},   // top right
    {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // bottom right
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // bottom left
    {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},  // top left
};

static const unsigned int indices[] = {0, 1, 2, 0, 3, 2};
// clang-format on

double get_time_since_start(void) { // i should probably make a better system
                                    // for this because im currently calculating
                                    // time twice (delta time and this)
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
        program_error("Couldn't load video frame from: %s\n", path);
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
            program_error("Failed to compile vertex shader\n%s\n", infoLog);
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
            program_error("Failed to compile fragment shader\n%s\n", infoLog);
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
            program_error("Failed to link shaders!\n%s\n", infoLog);
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
    }

    // Texture
    xab_log(LOG_DEBUG, "Video dimensions: %dx%dpx\n", vid.vr_state.width,
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

void video_render(VideoRenderer_t *vid, int x, int y, int width, int height,
                  camera_t *camera) {
    // read the frame
    static double pt_sec = 0.0f;

    const double tss = get_time_since_start();
    bool skip_decode = false;
    if (pt_sec > tss) { // wait for time to catch up
        skip_decode = true;
        xab_log(LOG_DEBUG, "skipped\n");
    } else if (pt_sec - tss <
               0) { // catch up frame to time
                    // todo (i think this is supposed to be framedropping)
    }

    int64_t pts;
    // upload texture to gpu
    // frame_idx: is used to copy pixels from a PBO to a texture
    // next_frame_idx: is used to update pixels in the other PBO
    const unsigned int frame_idx = 0;
    const unsigned int next_frame_idx = 1;

    // upload to texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vid->texture_id);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vid->pbos[frame_idx]);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vid->vr_state.width,
                    vid->vr_state.height, GL_RGB, GL_UNSIGNED_BYTE, 0);

    // decode next frame
    if (!skip_decode) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, vid->pbos[next_frame_idx]);

        // TODO: map on setup, unmap on cleanup

        // Note that glMapBuffer() causes sync issue. If GPU is working with
        // this buffer, glMapBuffer() will wait until GPU to finish its job. To
        // avoid waiting, you can call first glBufferData() with NULL pointer
        // before glMapBuffer(). If you do that, the previous data in PBO will
        // be discarded and glMapBuffer() returns a new allocated pointer
        // immediately even if GPU is still working with the previous data.
        GLCALL(glBufferData(GL_PIXEL_UNPACK_BUFFER,
                            vid->vr_state.frame_size_bytes, 0, GL_STREAM_DRAW));
        GLubyte *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        if (ptr) {
            if (!video_reader_read_frame(&vid->vr_state, ptr, &pts)) {
                program_error("Couldn't louad video frame\n");
                exit(EXIT_FAILURE);

                pt_sec = pts * av_q2d(vid->vr_state.time_base);
            }

            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }
    }

    // swap the pixel buffers (with the id)
    const unsigned int temp = vid->pbos[next_frame_idx];
    vid->pbos[next_frame_idx] = vid->pbos[frame_idx];
    vid->pbos[frame_idx] = temp;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); // unbind when done

    // now its time to start rendering

    // texture stuff
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    vid->config.pixelated ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    vid->config.pixelated ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

    glActiveTexture(GL_TEXTURE0);

    // shader stuff
    GLCALL(glUseProgram(vid->shader_program));
    glUniform1f(glGetUniformLocation(vid->shader_program, "Time"),
                get_time_since_start());

    bool all_identity = true;

#ifdef HAVE_LIBCGLM
    all_identity = false;
    if (width == 0 || height == 0) {
        all_identity = true;
    }

    if (!all_identity) {
        // projection matrix
        glUniformMatrix4fv(
            glGetUniformLocation(vid->shader_program, "ortho_proj"), 1,
            GL_FALSE, (const GLfloat *)camera->ortho);

        // view matrix
        glUniformMatrix4fv(glGetUniformLocation(vid->shader_program, "view"), 1,
                           GL_FALSE, (const GLfloat *)camera->view);

        // model matrix
        mat4 model = GLM_MAT4_IDENTITY_INIT;

        vec4 move = {x + (width / 2.f), y + (height / 2.f), 0.0f, 0.0f};
        glm_translate(model, move);

        vec4 da_scaler = {width / 2.f, height / 2.f, 1.0f, 1.0f};
        glm_scale(model, da_scaler);

        glUniformMatrix4fv(glGetUniformLocation(vid->shader_program, "model"),
                           1, GL_FALSE, (const GLfloat *)model);

        // don't flip the texture (cuz ortho already does)
        glUniform1i(glGetUniformLocation(vid->shader_program, "flip"), 0);
    }
#endif

    if (all_identity) {
        // use identity matrix instead the actual matrices
        float identity_matrix[16];
        mat4_identity_nocglm(identity_matrix);

        // projection matrix
        glUniformMatrix4fv(
            glGetUniformLocation(vid->shader_program, "ortho_proj"), 1,
            GL_FALSE, (const GLfloat *)identity_matrix);

        // view matrix
        glUniformMatrix4fv(glGetUniformLocation(vid->shader_program, "view"), 1,
                           GL_FALSE, (const GLfloat *)identity_matrix);

        // model matrix
        glUniformMatrix4fv(glGetUniformLocation(vid->shader_program, "model"),
                           1, GL_FALSE, (const GLfloat *)identity_matrix);

        // flip the texture
        glUniform1i(glGetUniformLocation(vid->shader_program, "flip"), 1);
    }

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
