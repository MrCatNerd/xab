#pragma once

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
#include <pthread.h>
#include <stdbool.h>

#include "packet_queue.h"

// NOTE: to future me, don't deadlock urself with pq mutex + decoder mutex pls
typedef struct Decoder {
        packet_queue_t pq;
        /// source (video) width and height
        unsigned int vwidth, vheight;
        /// target width and height
        unsigned int twidth, theight;

        AVFormatContext *av_format_ctx;
        const AVCodec *av_codec;
        struct AVCodecContext *av_codec_ctx;
        struct AVCodecParameters *av_codecpar;
        struct SwsContext *sws_scaler_ctx;
        int video_stream_idx;

        AVStream *video;
        AVFrame *av_frame;
        AVFrame *raw_av_frame;
        AVPacket *av_packet;
        size_t frame_size_bytes;

        int64_t start_time;
        double pt_sec;
        double time_base;

        void (*callback_func)(AVFrame *frame, void *callback_ctx);
        void *callback_ctx;

        pthread_mutex_t mutex;
        pthread_cond_t cond;
        pthread_t packet_worker_tid;
        bool dead;
} Decoder_t;

void decoder_init(Decoder_t *dst_dec, const char *path, unsigned int width,
                  unsigned int height, bool pixelated,
                  void (*callback_func)(AVFrame *frame, void *callback_ctx),
                  void *callback_ctx);

void decoder_decode(Decoder_t *dec);
void decoder_destroy(Decoder_t *dec);
