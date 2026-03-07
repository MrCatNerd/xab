#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavformat/avio.h>
#include <libavutil/pixfmt.h>
#include <pthread.h>
#include <stdbool.h>

#include "video/video_reader_interface.h"
#include "hwaccel/hwdec.h"
#include "picture_queue.h"
#include "packet_queue.h"

// NOTE: to future me, don't deadlock urself with pq mutex + decoder mutex pls
typedef struct Decoder {
        /// hardware decoding context (set to NULL if no hw accel)
        DecoderHW_ctx_t *hw_ctx;

        packet_queue_t pacq;
        picture_queue_t picq;
        /// video's width and height
        unsigned int vwidth, vheight;

        AVFormatContext *av_format_ctx;
        AVCodec *av_codec;
        struct AVCodecContext *av_codec_ctx;
        struct AVCodecParameters *av_codecpar;
        int video_stream_idx;

        AVStream *video;
        AVFrame *av_frame;
        AVFrame *av_pass_frame;
        AVPacket *av_packet;

        double pt_sec;
        double time_base;

        void (*callback_func)(AVFrame *frame, void *callback_ctx);
        void *callback_ctx;

        // packet queue thread
        pthread_mutex_t packet_mutex;
        pthread_cond_t picture_cond;
        pthread_t packet_worker_tid;
        bool packet_dead;

        // picture queue thread
        pthread_mutex_t picture_mutex;
        pthread_cond_t packet_cond;
        pthread_t picture_worker_tid;
        bool picture_dead;
} Decoder_t;

void decoder_init(Decoder_t *dst_dec, const char *path,
                  void (*callback_func)(AVFrame *frame, void *callback_ctx),
                  void *callback_ctx, enum VR_HW_ACCEL hw_accel);

void decoder_decode(Decoder_t *dec);
void decoder_destroy(Decoder_t *dec);
