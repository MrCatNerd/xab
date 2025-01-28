#include "pch.h"
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
#include "logger.h"
#include "utils.h"

// TODO: basic error handling

#define VR_INTERNAL(vrs) ((VRStateInternal_t *)vrs)
typedef struct VRStateInternal {
        unsigned int vwidth, vheight;

        // opengl things
        unsigned int pbos[2];
        unsigned int texture_id;

        // ffmpeg things
        double pt_sec;
        AVRational time_base;
        size_t frame_size_bytes;

        int video_stream_idx;
        AVFormatContext *av_format_ctx;
        const AVCodec *av_codec;
        struct AVCodecContext *av_codec_ctx;
        struct SwsContext *sws_scaler_ctx;

        AVStream *video;

        AVFrame *av_frame;
        AVPacket *av_packet;

        unsigned char internal_data[64]; // todo, change this
} VRStateInternal_t;

static double get_time_since_start(void);

// todo: smh like that
struct packet_list {
        AVPacket packet;
        struct packet_list *next;
};

struct packet_queue {
        struct packet_list *first_packet;
        struct packet_list *last_packet;
        int packet_count;
        int size;
};

VideoReaderState_t open_video(const char *path,
                              VideoReaderRenderConfig_t vr_config,
                              ShaderCache_t *scache) {
    (void)scache;
    VideoReaderState_t state = {.path = path,
                                .vrc = vr_config,
                                .internal =
                                    calloc(sizeof(VRStateInternal_t), 1)};

    VRStateInternal_t *internal_state = VR_INTERNAL(state.internal);

    // everything else is set to 0 because of calloc
    internal_state->video_stream_idx = -1;

    if (state.vrc.hw_accel == VR_HW_ACCEL_YES) {
        xab_log(LOG_WARN,
                "'xab_custom' (ffmpeg) video reader does not currently "
                "support hardware acceleration\n");
        state.vrc.hw_accel = VR_HW_ACCEL_NO;
    }
    if (state.vrc.gl_internal_format != GL_RGBA &&
        state.vrc.gl_internal_format != GL_RGB) {
        xab_log(LOG_WARN,
                "'xab_custom' (ffmpeg) video reader does not currently "
                "support any texture format other than RGB/RGBA");
        state.vrc.gl_internal_format = GL_RGB;
        // currently im just ignoring the alpha channel but i might not in the
        // future if i won't be not lazy
    }

    xab_log(LOG_DEBUG, "Reading file: %s\n", path);
    internal_state->av_format_ctx = avformat_alloc_context();
    if (internal_state->av_format_ctx == NULL) {
        xab_log(LOG_ERROR, "Couldn't create AVFormatContext\n");
    }

    if (avformat_open_input(&internal_state->av_format_ctx, path, NULL, NULL) !=
        0) {
        xab_log(LOG_ERROR, "Couldn't open video file: %s\n", path);
    }

    avformat_find_stream_info(internal_state->av_format_ctx, NULL);

    xab_log(LOG_VERBOSE, "File format long name: %s\n",
            internal_state->av_format_ctx->iformat->long_name);

    AVCodecParameters *av_codec_params = NULL;
    AVCodec *av_codec = NULL;
    internal_state->video_stream_idx = -1;

    // find the first valid video stream inside the file
    for (unsigned int i = 0; i < internal_state->av_format_ctx->nb_streams;
         ++i) {
        AVStream *stream = internal_state->av_format_ctx->streams[i];

        av_codec_params = stream->codecpar;
        av_codec = (AVCodec *)avcodec_find_decoder(av_codec_params->codec_id);

        if (!av_codec) {
            xab_log(LOG_WARN, "Can't find a codec on stream #%d", i);
            continue;
        }

        // i could mess around with audio too
        // if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
        //     continue;
        // }

        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            internal_state->video_stream_idx = i;
            internal_state->vwidth = av_codec_params->width;
            internal_state->vheight = av_codec_params->height;
            internal_state->time_base = stream->time_base;
            break;
        }
    }

    if (internal_state->video_stream_idx == -1) {
        xab_log(LOG_ERROR,
                "Couldn't find a valid video stream inside file: %s\n", path);
    }

    // setup a codec context for the decoder
    internal_state->av_codec_ctx = avcodec_alloc_context3(av_codec);
    if (!internal_state->av_format_ctx) {
        xab_log(LOG_ERROR, "Couldn't allocate AVCodecContext\n");
    }

    if (avcodec_parameters_to_context(internal_state->av_codec_ctx,
                                      av_codec_params) < 0) {
        xab_log(LOG_ERROR, "Couldn't initialize AVCodecContext\n");
    }
    if (avcodec_open2(internal_state->av_codec_ctx, av_codec, NULL) < 0) {
        xab_log(LOG_ERROR, "Couldn't open codec\n");
    }

    if (internal_state->av_codec_ctx->hwaccel != NULL) {
        xab_log(LOG_VERBOSE,
                "HW acceleration in use for video "
                "reading: %s\n",
                internal_state->av_codec_ctx->hwaccel->name);
    } else {
        xab_log(LOG_VERBOSE, "no HW acceleration in "
                             "use for video decoding :( good luck "
                             "cpu\n");
    }

    internal_state->av_frame = av_frame_alloc();
    if (!internal_state->av_frame) {
        xab_log(LOG_FATAL, "Couldn't allocate AVFrame\n");
    }

    internal_state->av_packet = av_packet_alloc();
    if (!internal_state->av_packet) {
        xab_log(LOG_FATAL, "Couldn't allocate AVPacket\n");
    }

    internal_state->internal_data[0] =
        internal_state->av_format_ctx->streams[internal_state->video_stream_idx]
            ->start_time &
        0xFFFFFFFFFFFFFFFF;

    internal_state->frame_size_bytes = state.vrc.width * state.vrc.height * 3;
    xab_log(LOG_DEBUG, "Frame size %zuMB\n",
            internal_state->frame_size_bytes / 1048576);

    // PBOs
    xab_log(LOG_DEBUG, "Generating PBOs\n");
    glGenBuffers(2, internal_state->pbos);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, internal_state->pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, internal_state->frame_size_bytes,
                     0, GL_STREAM_DRAW);
    }

    // Texture
    xab_log(LOG_DEBUG, "Creating video texture: %dx%dpx\n",
            internal_state->vwidth, internal_state->vheight);
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

    // unbind stuff
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return state;
}

void render_video(VideoReaderState_t *state) {
    TracyCZoneNC(tracy_ctx, "VIDEO_RENDER", TRACY_COLOR_GREEN, true);

    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    // swap the PBOs each frame cuz async
    const unsigned int temp = internal_state->pbos[0];
    internal_state->pbos[0] = internal_state->pbos[1];
    internal_state->pbos[1] = temp;

    // upload pbo(1) to texture
    glBindTexture(GL_TEXTURE_2D, internal_state->texture_id);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, internal_state->pbos[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, state->vrc.width, state->vrc.height,
                    GL_RGB, GL_UNSIGNED_BYTE, 0);

    // map pbo(0) cuz async good
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, internal_state->pbos[0]);

    // TODO: map on setup, unmap on cleanup
    // TODO: GL_MAP_COHERENT_BIT, GL_MAP_PERSISTENT_BIT, and check for
    // GL_MAP_COHERENT_BIT support somehow (i think its from
    // OpenGL 4.4+), unless having two pbos prevents that

    // Note that glMapBuffer() causes sync issue. If the GPU is working with
    // this buffer, glMapBuffer() will wait until the GPU finishes its job. To
    // avoid waiting, you can call first glBufferData() with a NULL pointer
    // before glMapBuffer(). If you do that, the previous data in the PBO will
    // be discarded and glMapBuffer() returns a new allocated pointer
    // immediately even if the GPU is still working with the previous data.
    glBufferData(GL_PIXEL_UNPACK_BUFFER, internal_state->frame_size_bytes, 0,
                 GL_STREAM_DRAW);

    // map to pixel buffer to a pointer so we can to it from ffmpeg
    GLubyte *mapped_pbuffer =
        (GLubyte *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (!mapped_pbuffer) {
        // use goto? nah
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        xab_log(LOG_FATAL, "Unable to map video PBO\n");
        return;
    }

    // now actually read the video:

    // get some members
    AVFormatContext *av_format_ctx = internal_state->av_format_ctx;
    AVCodecContext *av_codec_ctx = internal_state->av_codec_ctx;
    int video_stream_idx = internal_state->video_stream_idx;
    AVFrame *av_frame = internal_state->av_frame;
    AVPacket *av_packet = internal_state->av_packet;

    int response;
    while (av_read_frame(av_format_ctx, av_packet) >= 0) {
        if (av_packet->stream_index != video_stream_idx) {
            av_packet_unref(av_packet);
            continue;
        }

        response = avcodec_send_packet(av_codec_ctx, av_packet);
        if (response < 0) {
            xab_log(LOG_ERROR, "Failed to decode packet %s\n",
                    av_err2str(response));
            return;
        }

        response = avcodec_receive_frame(av_codec_ctx, av_frame);

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(av_packet);
            continue;
        } else if (response < 0) {
            av_packet_unref(av_packet);
            xab_log(LOG_ERROR, "Failed to decode packet: %s\n",
                    av_err2str(response));
            return;
        }

        av_packet_unref(av_packet);
        break;
    }

    // TODO: initialize sws context on open_video
    // convert YUV to RGB24 and get the pixel data (no alpha so 24bpp instead of
    // 32bpp)
    if (!internal_state->sws_scaler_ctx) {
        // for pixelated option
        enum SwsFlags flags = -1;
        if (state->vrc.pixelated)
            flags = SWS_POINT;
        else
            flags = SWS_FAST_BILINEAR;
        Assert((int)flags != -1 && "Program is stoooopid");

        internal_state->sws_scaler_ctx = sws_getContext(
            internal_state->vwidth, internal_state->vheight,
            av_codec_ctx->pix_fmt, state->vrc.width, state->vrc.height,
            AV_PIX_FMT_RGB24, flags, NULL, NULL, NULL);

        if (!internal_state->sws_scaler_ctx) {
            xab_log(LOG_ERROR, "Failed to create SwsContext scaler");
            return;
        }
    }
    if (!internal_state->sws_scaler_ctx) {
        xab_log(LOG_ERROR, "Couldn't initialize SwsContext scaler\n");
        return;
    }

    uint8_t *dest[2] = {mapped_pbuffer, NULL};
    int dest_linesize[2] = {state->vrc.width * 3, 0}; // width * bpp

    if (sws_scale(internal_state->sws_scaler_ctx,
                  (const uint8_t *const *)av_frame->data, av_frame->linesize, 0,
                  av_frame->height, dest, dest_linesize) <= 0) {
        xab_log(LOG_ERROR, "Failed to scale YUV to RGB");
        return;
    }

    // get the time in seconds
    internal_state->pt_sec = av_frame->pts * av_q2d(internal_state->time_base);

    xab_log(LOG_VERBOSE, "video time: %f | real time: %f | %ld/%ld\n",
            internal_state->pt_sec, get_time_since_start(), av_frame->pts,
            av_format_ctx->streams[video_stream_idx]->duration);

    // TODO: looping
    /* if (av_frame->pts == last_pts) {
        xab_log(LOG_VERBOSE, "looping video\n");
        const int64_t start_time =
            (int64_t)(internal_state->internal_data[0] & 0xFFFFFFFFFFFFFFFF);
        av_seek_frame(av_format_ctx, video_stream_idx,
                      start_time != AV_NOPTS_VALUE ? start_time : 0,
                      AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(av_codec_ctx);
    } */

    // unmap stuff
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    TracyCZoneEnd(tracy_ctx);

    // profit
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
    sws_freeContext(internal_state->sws_scaler_ctx);
    avformat_close_input(&internal_state->av_format_ctx);
    avformat_free_context(internal_state->av_format_ctx);
    av_frame_free(&internal_state->av_frame);
    av_packet_free(&internal_state->av_packet);
    avcodec_free_context(&internal_state->av_codec_ctx);

    // cleanup opengl things
    glDeleteBuffers(2, internal_state->pbos);
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
