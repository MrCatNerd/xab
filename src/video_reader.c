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
#include <stdio.h>

#include "video_reader.h"
#include "logging.h"

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

    // get members
    AVFormatContext **av_format_ctx = &state->av_format_ctx;
    AVCodecContext **av_codec_ctx = &state->av_codec_ctx;
    int *video_stream_idx = &state->video_stream_idx;
    AVFrame **av_frame = &state->av_frame;
    AVPacket **av_packet = &state->av_packet;
    struct SwsContext *sws_scaler_ctx = state->sws_scaler_ctx;

    VLOG("-- Reading file: %s\n", path);
    // open file
    *av_format_ctx = avformat_alloc_context();
    if (av_format_ctx == NULL) {
        fprintf(stderr, "Couldn't create AVFormatContext\n");
        return false;
    }

    if (avformat_open_input(av_format_ctx, path, NULL, NULL) != 0) {
        fprintf(stderr, "Couldn't open video file: %s\n", path);
        return false;
    }
    VLOG("-- File format long name: %s\n",
         (*av_format_ctx)->iformat->long_name);

    AVCodecParameters *av_codec_params = NULL;
    AVCodec *av_codec = NULL;
    *video_stream_idx = -1;

    // find the first valid video stream inside the file
    for (int i = 0; i < (*av_format_ctx)->nb_streams; ++i) {
        AVStream *stream = (*av_format_ctx)->streams[i];

        av_codec_params = (*av_format_ctx)->streams[i]->codecpar;
        av_codec = avcodec_find_decoder(av_codec_params->codec_id);

        if (!av_codec) {
            LOG("-- Warning: can't find a codec on stream #%d", i);
            continue;
        }

        // i could mess around with audio too
        // if (av_codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
        //     continue;
        // }

        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            *video_stream_idx = i;
            state->width = av_codec_params->width;
            state->height = av_codec_params->height;
            state->time_base = (*av_format_ctx)->streams[i]->time_base;
            break;
        }
    }

    if (*video_stream_idx == -1) {
        fprintf(stderr, "Couldn't find a valid video stream inside file: %s\n",
                path);
        return false;
    }

    // setup a codec context for the decoder
    *av_codec_ctx = avcodec_alloc_context3(av_codec);
    if (!av_codec_ctx) {
        fprintf(stderr, "Couldn't allocate AVCodecContext\n");
        return false;
    }

    if (avcodec_parameters_to_context(*av_codec_ctx, av_codec_params) < 0) {
        fprintf(stderr, "Couldn't initialize AVCodecContext\n");
        return false;
    }
    if (avcodec_open2(*av_codec_ctx, av_codec, NULL) < 0) {
        fprintf(stderr, "Couldn't open codec\n");
        return false;
    }

    // av_codec_ctx->thread_count = 1;
    *av_frame = av_frame_alloc();
    if (!av_frame) {
        fprintf(stderr, "Couldn't allocate AVFrame\n");
    }

    *av_packet = av_packet_alloc();
    if (!av_packet) {
        fprintf(stderr, "Couldn't allocate AVPacket\n");
    }

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
    struct SwsContext *sws_scaler_ctx = state->sws_scaler_ctx;

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
            fprintf(stderr, "Failed to decode packet %s\n",
                    av_err2str(response));
            return false;
        }

        av_packet_unref(av_packet);
        break;
    }

    // convert YUV to RGB0 and get the pixel data
    if (!sws_scaler_ctx) {
        sws_scaler_ctx =
            sws_getContext(state->width, state->height, av_codec_ctx->pix_fmt,
                           state->width, state->height, AV_PIX_FMT_RGB0,
                           SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }
    if (!sws_scaler_ctx) {
        fprintf(stderr, "Couldn't initialize SwsContext scaler\n");
        return false;
    }

    uint8_t *dest[4] = {pbuffer, NULL, NULL, NULL};
    int dest_linesize[4] = {state->width * 4, 0, 0, 0};

    *pts = av_frame->pts;

    sws_scale(sws_scaler_ctx, (const uint8_t *const *)av_frame->data,
              av_frame->linesize, 0, av_frame->height, dest, dest_linesize);

    // av_seek_frame(av_format_ctx, video_stream_idx, 0, NULL);
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
