#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavutil/pixfmt.h>

#include "shader_cache.h"
#include "video_reader_interface.h"
#include "ffmpeg_reader/decoder.h"
#include "logger.h"

static void decoder_callback_ctx(AVFrame *frame, void *callback_ctx);

#define VR_INTERNAL(vrs) ((VRStateInternal_t *)vrs)
typedef struct VRStateInternal {
        Decoder_t decoder;
        unsigned int texture_id;
} VRStateInternal_t;

static double get_time_since_start(void);

VideoReaderState_t open_video(const char *path,
                              VideoReaderRenderConfig_t vr_config,
                              ShaderCache_t *scache) {
    (void)scache;
    VideoReaderState_t state = {.path = path,
                                .vrc = vr_config,
                                .internal =
                                    calloc(sizeof(VRStateInternal_t), 1)};

    VRStateInternal_t *internal_state = VR_INTERNAL(state.internal);

    if (state.vrc.gl_internal_format != GL_RGBA &&
        state.vrc.gl_internal_format != GL_RGB) {
        xab_log(LOG_WARN, "ffmpeg video reader does not currently "
                          "support any texture format other than RGB/RGBA");
        state.vrc.gl_internal_format = GL_RGB;
        // currently im just ignoring the alpha channel but i might not in the
        // future if i won't be not lazy
    }

    xab_log(LOG_DEBUG, "Reading video file: %s\n", path);
    decoder_init(&internal_state->decoder, path, state.vrc.width,
                 state.vrc.height, state.vrc.pixelated, &decoder_callback_ctx,
                 internal_state, vr_config.hw_accel);

    // Texture
    xab_log(LOG_DEBUG, "Creating video texture: %dx%dpx\n",
            internal_state->decoder.twidth, internal_state->decoder.theight);
    glGenTextures(1, &internal_state->texture_id);
    glBindTexture(GL_TEXTURE_2D, internal_state->texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    state.vrc.pixelated ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    state.vrc.pixelated ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, state.vrc.gl_internal_format,
                 state.vrc.width, state.vrc.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 0); // set all to black
    glBindTexture(GL_TEXTURE_2D, 0);

    return state;
}

void render_video(VideoReaderState_t *state) {
    TracyCZoneNC(tracy_ctx, "VIDEO_RENDER", TRACY_COLOR_GREEN, true);

    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    decoder_decode(&internal_state->decoder);

    TracyCZoneEnd(tracy_ctx);

    // profit
}

static void decoder_callback_ctx(AVFrame *frame, void *callback_ctx) {
    VRStateInternal_t *internal_state = VR_INTERNAL(callback_ctx);
    glBindTexture(GL_TEXTURE_2D, internal_state->texture_id);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame->width, frame->height, GL_RGB,
                    GL_UNSIGNED_BYTE, frame->data[0]);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void close_video(VideoReaderState_t *state, ShaderCache_t *scache) {
    (void)scache;
    xab_log(LOG_VERBOSE, "Closing video: %s\n", state->path);
    if (!state || !state->internal) {
        xab_log(LOG_ERROR, "No video reader state of video %s\n", state->path);
        return;
    }
    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    // cleanup ffmpeg things
    decoder_destroy(&internal_state->decoder);

    // cleanup opengl things
    glDeleteTextures(1, &internal_state->texture_id);

    free(state->internal);
}

unsigned int get_video_ogl_texture(VideoReaderState_t *state) {
    // hehe no framebuffer get rekt

    return VR_INTERNAL(state->internal)->texture_id;
}

// idk if this is the right approach, but it works... sort of
static double get_time_since_start(void) {
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

void report_swap_video(VideoReaderState_t *state) {
    (void)
        state; // im too much of a noob to implement this, and this is optional
}
