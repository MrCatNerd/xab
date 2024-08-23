#pragma once

#include <libavformat/avformat.h>

typedef struct VideoReaderState {
        // public
        int width;
        int height;
        AVRational time_base;

        // internal
        AVFormatContext *av_format_ctx;
        AVCodecContext *av_codec_ctx;
        int video_stream_idx;
        AVFrame *av_frame;
        AVPacket *av_packet;
        struct SwsContext *sws_scaler_ctx;
} VideoReaderState_t;

bool video_reader_open(VideoReaderState_t *state, const char *path);
bool video_reader_read_frame(VideoReaderState_t *state, uint8_t *pbuffer,
                             int64_t *pts);
bool video_reader_close(VideoReaderState_t *state);
