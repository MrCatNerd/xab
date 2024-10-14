#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <libavformat/avformat.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>

typedef struct VideoReaderState {
        // public
        int width;
        int height;
        AVRational time_base;
        size_t frame_size_bytes;

        // internal
        int video_stream_idx;
        AVFormatContext *av_format_ctx;
        const AVCodec *av_codec;
        struct AVCodecContext *av_codec_ctx;
        struct SwsContext *sws_scaler_ctx;

        AVStream *video;

        AVFrame *av_frame;
        AVPacket *av_packet;

        unsigned char internal_data[64];
} VideoReaderState_t;

bool video_reader_open(VideoReaderState_t *state, const char *path);
bool video_reader_read_frame(VideoReaderState_t *state, uint8_t *pbuffer,
                             int64_t *pts);
bool video_reader_close(VideoReaderState_t *state);
