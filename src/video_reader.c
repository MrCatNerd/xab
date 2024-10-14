#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

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

#include "video_reader.h"
#include "logging.h"

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

bool video_reader_open(VideoReaderState_t *state, const char *path) {
    // remember to set everything to NULL
    state->width = -1;
    state->height = -1;

    state->av_format_ctx = NULL;
    state->av_codec_ctx = NULL;
    state->video_stream_idx = -1;
    state->av_frame = NULL;
    state->av_packet = NULL;
    state->sws_scaler_ctx = NULL;

    VLOG("-- Reading file: %s\n", path);
    // open file
    state->av_format_ctx = avformat_alloc_context();
    if (state->av_format_ctx == NULL) {
        fprintf(stderr, "Couldn't create AVFormatContext\n");
        return false;
    }

    if (avformat_open_input(&state->av_format_ctx, path, NULL, NULL) != 0) {
        fprintf(stderr, "Couldn't open video file: %s\n", path);
        return false;
    }

    avformat_find_stream_info(state->av_format_ctx, NULL);

    VLOG("-- File format long name: %s\n",
         state->av_format_ctx->iformat->long_name);

    AVCodecParameters *av_codec_params = NULL;
    AVCodec *av_codec = NULL;
    state->video_stream_idx = -1;

    // find the first valid video stream inside the file
    for (unsigned int i = 0; i < state->av_format_ctx->nb_streams; ++i) {
        AVStream *stream = state->av_format_ctx->streams[i];

        av_codec_params = stream->codecpar;
        av_codec = (AVCodec *)avcodec_find_decoder(av_codec_params->codec_id);

        if (!av_codec) {
            LOG("-- Warning: can't find a codec on stream #%d", i);
            continue;
        }

        // i could mess around with audio too
        // if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
        //     continue;
        // }

        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            state->video_stream_idx = i;
            state->width = av_codec_params->width;
            state->height = av_codec_params->height;
            state->time_base = stream->time_base;
            break;
        }
    }

    if (state->video_stream_idx == -1) {
        fprintf(stderr, "Couldn't find a valid video stream inside file: %s\n",
                path);
        return false;
    }

    // setup a codec context for the decoder
    state->av_codec_ctx = avcodec_alloc_context3(av_codec);
    if (!state->av_format_ctx) {
        fprintf(stderr, "Couldn't allocate AVCodecContext\n");
        return false;
    }

    if (avcodec_parameters_to_context(state->av_codec_ctx, av_codec_params) <
        0) {
        fprintf(stderr, "Couldn't initialize AVCodecContext\n");
        return false;
    }
    if (avcodec_open2(state->av_codec_ctx, av_codec, NULL) < 0) {
        fprintf(stderr, "Couldn't open codec\n");
        return false;
    }

    if (state->av_codec_ctx->hwaccel != NULL) {
        LOG("-- HW acceleration in use for video reading: %s\n",
            state->av_codec_ctx->hwaccel->name);
    } else {
        LOG("-- no HW acceleration in use for video reading:( good luck cpu\n");
    }

    state->av_codec_ctx->thread_count = 4;
    state->av_frame = av_frame_alloc();
    if (!state->av_frame) {
        fprintf(stderr, "Couldn't allocate AVFrame\n");
    }

    state->av_packet = av_packet_alloc();
    if (!state->av_packet) {
        fprintf(stderr, "Couldn't allocate AVPacket\n");
    }

    state->internal_data[0] =
        state->av_format_ctx->streams[state->video_stream_idx]->start_time &
        0xFFFFFFFFFFFFFFFF;

    state->frame_size_bytes = state->width * state->height * 3;
    LOG("-- Frame size %zuMB\n", state->frame_size_bytes / 1048576);

    return true;
}
bool video_reader_read_frame(VideoReaderState_t *state, uint8_t *pbuffer,
                             int64_t *pts) {
    // get members
    AVFormatContext *av_format_ctx = state->av_format_ctx;
    AVCodecContext *av_codec_ctx = state->av_codec_ctx;
    int video_stream_idx = state->video_stream_idx;
    AVFrame *av_frame = state->av_frame;
    AVPacket *av_packet = state->av_packet;

    int response;
    while (av_read_frame(av_format_ctx, av_packet) >= 0) {
        if (av_packet->stream_index != video_stream_idx) {
            av_packet_unref(av_packet);
            continue;
        }

        response = avcodec_send_packet(av_codec_ctx, av_packet);
        if (response < 0) {
            fprintf(stderr, "Failed to decode packet %s\n",
                    av_err2str(response));
            return false;
        }

        response = avcodec_receive_frame(av_codec_ctx, av_frame);

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(av_packet);
            continue;
        } else if (response < 0) {
            av_packet_unref(av_packet);
            fprintf(stderr, "Failed to decode packet %s\n",
                    av_err2str(response));
            return false;
        }

        av_packet_unref(av_packet);
        break;
    }

    // convert YUV to RGB24 and get the pixel data (no alpha so 3 bits)
    if (!state->sws_scaler_ctx) {
        state->sws_scaler_ctx =
            sws_getContext(state->width, state->height, av_codec_ctx->pix_fmt,
                           state->width, state->height, AV_PIX_FMT_RGB24,
                           SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }
    if (!state->sws_scaler_ctx) {
        fprintf(stderr, "Couldn't initialize SwsContext scaler\n");
        return false;
    }

    uint8_t *dest[2] = {pbuffer, NULL};
    int dest_linesize[2] = {state->width * 3, 0};

    *pts = av_frame->pts;

    sws_scale(state->sws_scaler_ctx, (const uint8_t *const *)av_frame->data,
              av_frame->linesize, 0, av_frame->height, dest, dest_linesize);
    static int64_t last_pts = -1;

    VLOG("%ld/%ld\n", av_frame->pts,
         av_format_ctx->streams[video_stream_idx]->duration);

    if (av_frame->pts == last_pts) {
        LOG("looping video\n");
        const int64_t start_time =
            (int64_t)(state->internal_data[0] & 0xFFFFFFFFFFFFFFFF);
        av_seek_frame(av_format_ctx, video_stream_idx,
                      start_time != AV_NOPTS_VALUE ? start_time : 0,
                      AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(av_codec_ctx);
    }

    last_pts = av_frame->pts;

    return true;
}

bool video_reader_close(VideoReaderState_t *state) {
    sws_freeContext(state->sws_scaler_ctx);
    avformat_close_input(&state->av_format_ctx);
    avformat_free_context(state->av_format_ctx);
    av_frame_free(&state->av_frame);
    av_packet_free(&state->av_packet);
    avcodec_free_context(&state->av_codec_ctx);

    return true;
}
