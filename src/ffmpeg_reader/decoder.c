#include "ffmpeg_reader/decoder.h"
#include "ffmpeg_reader/packet_queue.h"
#include "logger.h"
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *decoder_packet_worker(void *ctx);

void decoder_init(Decoder_t *dst_dec, const char *path, unsigned int width,
                  unsigned int height, bool pixelated,
                  void (*callback_func)(AVFrame *frame, void *callback_ctx),
                  void *callback_ctx) {
    // -- initalize struct --
    memset(dst_dec, 0, sizeof(*dst_dec));

    // set defaults
    dst_dec->video_stream_idx = -1;
    dst_dec->dead = false;

    // set target dimensions
    dst_dec->twidth = width;
    dst_dec->theight = height;

    // initialize packet queue
    xab_log(LOG_TRACE, "Decoder: Initalizing packet queue\n");
    dst_dec->pq = packet_queue_init(70, 128);

    // allocate packets and frames
    xab_log(LOG_TRACE, "Decoder: Allocating AVPackets and AVFrames\n");
    dst_dec->av_packet = av_packet_alloc();
    dst_dec->av_frame = av_frame_alloc();
    dst_dec->raw_av_frame = av_frame_alloc();
    // - check
    if (!dst_dec->av_packet) {
        xab_log(LOG_ERROR, "Decoder: Failed to allocate AVPacket: av_packet\n");
    }
    if (!dst_dec->av_frame) {
        xab_log(LOG_ERROR, "Decoder: Failed to allocate AVFrame: av_frame\n");
    }
    if (!dst_dec->raw_av_frame) {
        xab_log(LOG_ERROR,
                "Decoder: Failed to allocate AVFrame: raw_av_frame\n");
    }
    // - allocate memory for the scaled frame
    xab_log(LOG_TRACE, "Decoder: Allocating frame memory for raw frame\n");
    dst_dec->raw_av_frame->width = dst_dec->twidth;
    dst_dec->raw_av_frame->height = dst_dec->theight;
    dst_dec->raw_av_frame->format = AV_PIX_FMT_RGB24;
    if (av_frame_get_buffer(dst_dec->raw_av_frame, 32)) { // 32-byte alignment
        xab_log(LOG_ERROR, "Decoder: Failed to allocate frame buffer\n");
    }

    // set the callback function
    xab_log(LOG_TRACE, "Decoder: Setting callback functions\n");
    // - check
    if (!callback_func) {
        xab_log(LOG_INFO,
                "Decoder: callback function is NULL, it will be ignored...");
    }
    dst_dec->callback_func = callback_func;
    dst_dec->callback_ctx = callback_ctx;

    // -- initalize av context --
    xab_log(LOG_TRACE, "Decoder: Allocating context\n", path);
    dst_dec->av_format_ctx = avformat_alloc_context();
    if (dst_dec->av_format_ctx == NULL) {
        xab_log(LOG_ERROR, "Couldn't create AVFormatContext\n");
        // haha no error handling deal with it
    }

    // open da file or smh
    xab_log(LOG_TRACE, "Decoder: Opening video file: %s\n", path);
    if (avformat_open_input(&dst_dec->av_format_ctx, path, NULL, NULL) != 0) {
        xab_log(LOG_ERROR, "Couldn't open video file: %s\n", path);
    }

    // read stream information
    xab_log(LOG_TRACE, "Decoder: Finding stream information\n", path);
    if (avformat_find_stream_info(dst_dec->av_format_ctx, NULL) < 0) {
        xab_log(LOG_ERROR, "Unable to get stream info\n");
    }

    // find video stream
    xab_log(LOG_TRACE, "Decoder: Finding video stream index\n", path);
    dst_dec->video_stream_idx =
        av_find_best_stream(dst_dec->av_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                            &dst_dec->av_codec, 0);

    if (dst_dec->video_stream_idx < 0) {
        dst_dec->av_codec = NULL;
        xab_log(LOG_ERROR, "Unable to find any compatible video stream!\n");
    }

    // get the AVStream video
    dst_dec->video = dst_dec->av_format_ctx->streams[dst_dec->video_stream_idx];

    // get the codec parameters from the video
    dst_dec->av_codecpar = dst_dec->video->codecpar;
    if (!dst_dec->av_codecpar) {
        xab_log(LOG_ERROR, "Decoder: AVCodecParameters* is NULL!");
    }

    // get the video's dimensions from the codec params
    dst_dec->vwidth = dst_dec->av_codecpar->width;
    dst_dec->vheight = dst_dec->av_codecpar->height;

    // -- allocate a codec context and fill it --
    dst_dec->av_codec_ctx = avcodec_alloc_context3(dst_dec->av_codec);
    if (!dst_dec->av_format_ctx) {
        xab_log(LOG_ERROR, "Couldn't allocate AVCodecContext\n");
    }

    if (avcodec_parameters_to_context(dst_dec->av_codec_ctx,
                                      dst_dec->av_codecpar) < 0) {
        xab_log(LOG_ERROR, "Couldn't initialize AVCodecContext\n");
    }

    // TODO: hw_accel

    // -- open codec --
    if (avcodec_open2(dst_dec->av_codec_ctx, dst_dec->av_codec, NULL) < 0) {
        xab_log(LOG_ERROR, "Couldn't open codec\n");
    }

    // log some stuff
    xab_log(LOG_VERBOSE, "File format long name: %s\n",
            dst_dec->av_format_ctx->iformat->long_name);

    if (dst_dec->av_codec_ctx->hwaccel != NULL) {
        xab_log(LOG_VERBOSE,
                "Decoder: HW acceleration in use for video "
                "reading: %s\n",
                dst_dec->av_codec_ctx->hwaccel->name);
    } else {
        xab_log(LOG_VERBOSE, "Decoder: no HW acceleration in "
                             "use for video decoding :( good luck "
                             "cpu\n");
    }

    // -- create sws_scaler_ctx --
    // for pixelated option
    xab_log(LOG_TRACE, "Decoder: Creating sws frame scaler\n");
    enum SwsFlags flags = -1;
    if (pixelated)
        flags = SWS_POINT;
    else
        flags = SWS_FAST_BILINEAR;
    Assert((int)flags != -1 && "Program is stoooopid");

    dst_dec->sws_scaler_ctx = sws_getContext(
        dst_dec->vwidth, dst_dec->vheight, dst_dec->av_codec_ctx->pix_fmt,
        dst_dec->vwidth, dst_dec->vheight, AV_PIX_FMT_RGB24, flags, NULL, NULL,
        NULL);

    if (!dst_dec->sws_scaler_ctx) {
        xab_log(LOG_ERROR, "Failed to create SwsContext scaler");
    }

    // get time information
    xab_log(LOG_TRACE, "Decoder: Getting time information\n");
    dst_dec->time_base = av_q2d(dst_dec->video->time_base);
    dst_dec->start_time = dst_dec->video->start_time;

    // get the frame size
    dst_dec->frame_size_bytes = dst_dec->vwidth * dst_dec->vheight * 3;
    xab_log(LOG_DEBUG, "Decoder: Frame size %zuMB\n",
            dst_dec->frame_size_bytes / 1048576);

    xab_log(LOG_TRACE, "Decoder: Creating threads...\n");
    // initialize mutex and cond
    pthread_mutex_init(&dst_dec->mutex, NULL);
    pthread_cond_init(&dst_dec->cond, NULL);

    pthread_mutex_lock(&dst_dec->mutex);
    // create threads
    pthread_create(&dst_dec->packet_worker_tid, NULL, &decoder_packet_worker,
                   dst_dec);

    pthread_mutex_unlock(&dst_dec->mutex);
}

static void *decoder_packet_worker(void *ctx) {
    Decoder_t *dec = (Decoder_t *)ctx;
    AVPacket *packet = av_packet_alloc();

    while (!dec->dead) {
        pthread_mutex_lock(&dec->mutex);

        // enqueue packets
        av_read_frame(dec->av_format_ctx, packet);
        if (packet->stream_index != dec->video_stream_idx) {
            av_packet_unref(packet);
            pthread_mutex_unlock(&dec->mutex);
            continue;
        }

        pthread_cond_broadcast(&dec->cond);
        // xab_log(LOG_TRACE, "%d/%d packets\n", dec->pq.packet_count,
        //         dec->pq.size);

    // haha bad code practices go bRRR
    retry:
        if (!packet_queue_put(&dec->pq, packet)) {
            if (!dec->dead) {
                pthread_cond_wait(
                    &dec->cond,
                    &dec->mutex); // wait for the other thread to get
                                  // some packets from the queue

                goto retry;
            }
        }
        av_packet_unref(packet);

        pthread_mutex_unlock(&dec->mutex);
    }

    xab_log(LOG_DEBUG, "Decoder packet thread: Quitting\n");

    av_packet_free(&packet);

    pthread_exit(0);
}

void decoder_decode(Decoder_t *dec) {
    AVCodecContext *av_codec_ctx = dec->av_codec_ctx;
    int video_stream_idx = dec->video_stream_idx;
    AVFrame *av_frame = dec->av_frame;
    AVFrame *raw_av_frame = dec->raw_av_frame;
    AVPacket *av_packet = dec->av_packet;

    int response;
    while ((response = avcodec_receive_frame(av_codec_ctx, av_frame))) {
        pthread_cond_broadcast(&dec->cond);

    // haha bad code practices go bRRR
    retry:
        if (!packet_queue_get(&dec->pq, av_packet)) {
            if (!dec->dead) {
                pthread_mutex_lock(&dec->mutex);
                pthread_cond_wait(&dec->cond, &dec->mutex);
                pthread_mutex_unlock(&dec->mutex);

                goto retry;
            }
        }

        if (av_packet->stream_index != video_stream_idx) {
            av_packet_unref(av_packet);
            continue;
        }

        response = avcodec_send_packet(av_codec_ctx, av_packet);
        if (response < 0) {
            xab_log(LOG_ERROR, "Failed to decode packet: %s\n",
                    av_err2str(response));
            return;
        }

        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref(av_packet);
            continue;
        } else if (response < 0) {
            av_packet_unref(av_packet);
            xab_log(LOG_ERROR, "Failed to decode packet: %s\n",
                    av_err2str(response));
            continue;
        }

        av_packet_unref(av_packet);
    }

    // TODO: opengl YUV textures (fallback to swsscale if it's not YUV?)
    if (sws_scale(dec->sws_scaler_ctx, (const uint8_t *const *)av_frame->data,
                  av_frame->linesize, 0, av_frame->height, raw_av_frame->data,
                  raw_av_frame->linesize) <= 0) {
        xab_log(LOG_ERROR, "Failed to scale YUV to RGB");
        return;
    }

    // call the callback function
    if (dec->callback_func)
        (*dec->callback_func)(raw_av_frame, dec->callback_ctx);

    // TODO: looping
    // if (av_frame->pts == last_pts) {
    //     xab_log(LOG_VERBOSE, "looping video\n");
    //     av_seek_frame(av_format_ctx, video_stream_idx,
    //                   dec->start_time != AV_NOPTS_VALUE ? dec->start_time :
    //                   0, AVSEEK_FLAG_BACKWARD);
    //     avcodec_flush_buffers(av_codec_ctx);
    // }
}

void decoder_destroy(Decoder_t *dec) {
    pthread_mutex_lock(&dec->mutex);
    dec->dead = true;
    pthread_mutex_unlock(&dec->mutex);
    // broadcast so if they're waiting before we set dead to true, they will be
    // waken up
    pthread_cond_broadcast(&dec->cond);
    // wait for thread to terminate
    pthread_join(dec->packet_worker_tid, NULL);
    pthread_cond_destroy(&dec->cond);
    pthread_mutex_destroy(&dec->mutex);

    // destroy packet queue
    packet_queue_free(&dec->pq);

    // destroy allocated packets
    if (dec->av_packet)
        av_packet_free(&dec->av_packet);
    if (dec->av_frame)
        av_frame_free(&dec->av_frame);
    if (dec->raw_av_frame)
        av_frame_free(&dec->av_frame);

    // cleanup ffmpeg things
    if (dec->sws_scaler_ctx)
        sws_freeContext(dec->sws_scaler_ctx);
    if (dec->av_format_ctx) {
        avformat_close_input(&dec->av_format_ctx);
        avformat_free_context(dec->av_format_ctx);
    }
    if (dec->av_codec_ctx)
        avcodec_free_context(&dec->av_codec_ctx);
}
