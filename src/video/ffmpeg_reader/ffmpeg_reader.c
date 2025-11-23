#include "video/video_reader_interface.h"

#include <assert.h>
#include <epoxy/gl.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <stdlib.h>
#include <time.h>

#include "logger.h"
#include "render/image.h"
#include "render/shader_cache.h"
#include "render/texture.h"
#include "tracy.h"
#include "video/ffmpeg_reader/decoder.h"

static void decoder_callback_ctx(AVFrame *frame, void *callback_ctx);

#define VR_INTERNAL(vrs) ((VRStateInternal_t *)vrs)
typedef struct VRStateInternal {
        Decoder_t decoder;
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

    xab_log(LOG_DEBUG, "Creating video image: %fx%fpx\n",
            (int)(state.vrc.width * state.vrc.scale),
            (int)(state.vrc.height * state.vrc.scale));
    state.image = calloc(1, sizeof(Image_t));
    image_create(state.image, IMAGE_CSTD_YUV_UNKNOWN, IMAGE_CRANGE_JPEG,
                 (int)(state.vrc.width * state.vrc.scale),
                 (int)(state.vrc.height * state.vrc.scale),
                 state.vrc.pixelated);

    xab_log(LOG_DEBUG, "Reading video file: %s\n", path);
    decoder_init(&internal_state->decoder, path, &decoder_callback_ctx,
                 state.image, vr_config.hw_accel);

    return state;
}

void render_video(VideoReaderState_t *state) {
    TracyCZoneNC(tracy_ctx, "VIDEO_RENDER", TRACY_COLOR_GREEN, true);

    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    decoder_decode(&internal_state->decoder);

    TracyCZoneEnd(tracy_ctx);

    // profit
}

static void upload_texture_frame(Texture_t *texture, AVFrame *frame, int idx,
                                 bool uv) {
    unsigned char *data = frame->data[idx];
    if (!data)
        return;
    const int linesize = frame->linesize[idx];
    const int data_height = uv ? AV_CEIL_RSHIFT(frame->height, 1)
                               : frame->height; // U and V are half size

    if (linesize >= 0)
        subimage_texture(texture, 0, 0, data, linesize, data_height);
    else
        // when linesize is negative, pointer starts at last row
        subimage_texture(texture, 0, 0, data + linesize * (data_height - 1),
                         -linesize, data_height);
}

static void decoder_callback_ctx(AVFrame *frame, void *callback_ctx) {
    Image_t *image = callback_ctx;
    if (!image) // image is still uninitialized
        return;

    xab_log(LOG_TRACE, "Filling textures and shi\n");
    upload_texture_frame(&image->textures[0], frame, 0, false);
    upload_texture_frame(&image->textures[1], frame, 1, true);
    upload_texture_frame(&image->textures[2], frame, 2, true);
    switch (frame->colorspace) {
    default:
    case AVCOL_SPC_RESERVED:
    case AVCOL_SPC_UNSPECIFIED:
    case AVCOL_SPC_RGB: {
        char *name = NULL;
        // no budget for reflections lol
        switch (frame->colorspace) {
        case AVCOL_SPC_RGB:
            name = "AVCOL_SPC_RGB";
            break;
        case AVCOL_SPC_BT709:
            name = "AVCOL_SPC_BT709";
            break;
        default:
        case AVCOL_SPC_UNSPECIFIED:
            name = "AVCOL_SPC_UNSPECIFIED";
            break;
        case AVCOL_SPC_RESERVED:
            name = "AVCOL_SPC_RESERVED";
            break;
        case AVCOL_SPC_FCC:
            name = "AVCOL_SPC_FCC";
            break;
        case AVCOL_SPC_BT470BG:
            name = "AVCOL_SPC_BT470BG";
            break;
        case AVCOL_SPC_SMPTE170M:
            name = "AVCOL_SPC_SMPTE170M";
            break;
        case AVCOL_SPC_SMPTE240M:
            name = "AVCOL_SPC_SMPTE240M";
            break;
        // case AVCOL_SPC_YCOCG: // =AVCOL_SPC_YCGCO
        case AVCOL_SPC_YCGCO:
            name = "AVCOL_SPC_YCGCO";
            break;
        case AVCOL_SPC_BT2020_NCL:
            name = "AVCOL_SPC_BT2020_NCL";
            break;
        case AVCOL_SPC_BT2020_CL:
            name = "AVCOL_SPC_BT2020_CL";
            break;
        case AVCOL_SPC_SMPTE2085:
            name = "AVCOL_SPC_SMPTE2085";
            break;
        case AVCOL_SPC_CHROMA_DERIVED_NCL:
            name = "AVCOL_SPC_CHROMA_DERIVED_NCL";
            break;
        case AVCOL_SPC_CHROMA_DERIVED_CL:
            name = "AVCOL_SPC_CHROMA_DERIVED_CL";
            break;
        case AVCOL_SPC_ICTCP:
            name = "AVCOL_SPC_ICTCP";
            break;
        case AVCOL_SPC_IPT_C2:
            name = "AVCOL_SPC_IPT_C2";
            break;
        case AVCOL_SPC_YCGCO_RE:
            name = "AVCOL_SPC_YCGCO_RE";
            break;
        case AVCOL_SPC_YCGCO_RO:
            name = "AVCOL_SPC_YCGCO_RO";
            break;
        case AVCOL_SPC_NB:
            name = "AVCOL_SPC_NB";
            break;
        }
        assert(name != NULL && "Invalid pointer");
        xab_log(LOG_WARN,
                "Unsupported AV colorspace: %s, defaulting to BT709!\n", name);
    }
        /* fallthrough */
    case AVCOL_SPC_BT709:
        image->cstandard = IMAGE_CSTD_YUV_BT709;
        break;

    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        // bt601
        image->cstandard = IMAGE_CSTD_YUV_BT601;
        break;

    // TODO: AVCOL_SPC_BT2020_CL
    case AVCOL_SPC_BT2020_NCL:
        // bt2020
        image->cstandard = IMAGE_CSTD_YUV_BT2020;
        break;
    }
    switch (frame->color_range) {
    default:
    case AVCOL_RANGE_MPEG:
        image->crange = IMAGE_CRANGE_MPEG;
        break;
    case AVCOL_RANGE_JPEG:
        image->crange = IMAGE_CRANGE_JPEG;
        break;
    }
    unbind_texture();
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

    // cleanup image
    image_destroy_textures(state->image);
    free(state->image);
    state->image = NULL;

    free(state->internal);
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
