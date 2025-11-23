#include "video/ffmpeg_reader/decoder.h"

#include <libavcodec/codec.h>
#include <libavcodec/packet.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/rational.h>

// libav version info
#include <libavcodec/version.h>
#include <libavdevice/version.h>
#include <libavfilter/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "utils.h"
#include "video/ffmpeg_reader/hwaccel/hwdec.h"
#include "video/ffmpeg_reader/packet_queue.h"
#include "video/ffmpeg_reader/picture_queue.h"
#include "video/video_reader_interface.h"

static void *decoder_packet_worker(void *ctx);
static void *decoder_picture_worker(void *ctx);

void decoder_init(Decoder_t *dst_dec, const char *path,
                  void (*callback_func)(AVFrame *frame, void *callback_ctx),
                  void *callback_ctx, enum VR_HW_ACCEL hw_accel) {
    // -- initalize struct --
    memset(dst_dec, 0, sizeof(*dst_dec));

    // set defaults
    dst_dec->video_stream_idx = -1;

    // log some info about libav
    xab_log(
        LOG_DEBUG,
        "libavcodec v%d.%d.%d | libavformat v%d.%d.%d | libavutil v%d.%d.%d\n",
        // clang-format off
            LIBAVDEVICE_VERSION_MAJOR, LIBAVDEVICE_VERSION_MINOR, LIBAVDEVICE_VERSION_MICRO,
            LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO,
            LIBAVUTIL_VERSION_MAJOR  , LIBAVUTIL_VERSION_MINOR  , LIBAVUTIL_VERSION_MICRO
        // clang-format on
    );

    // initialize packet queue
    xab_log(LOG_TRACE, "Decoder: Initializing pcket queue\n");
    dst_dec->pacq = packet_queue_init(128);
    xab_log(LOG_TRACE, "Decoder: Initializing picture queue\n");
    dst_dec->picq = picture_queue_init(64);

    // allocate packets and frames
    xab_log(LOG_TRACE, "Decoder: Allocating AVPackets and AVFrames\n");
    dst_dec->av_packet = av_packet_alloc();
    dst_dec->av_frame = av_frame_alloc();
    dst_dec->av_pass_frame = av_frame_alloc();
    // - check
    if (!dst_dec->av_packet) {
        xab_log(LOG_ERROR, "Decoder: Failed to allocate AVPacket: av_packet\n");
    }
    if (!dst_dec->av_frame) {
        xab_log(LOG_ERROR, "Decoder: Failed to allocate AVFrame: av_frame\n");
    }
    if (!dst_dec->av_pass_frame) {
        xab_log(LOG_ERROR,
                "Decoder: Failed to allocate AVFrame: av_pass_frame\n");
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
                            (const AVCodec **)(&dst_dec->av_codec), 0);

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

    // hw accel stuff
    switch (hw_accel) {
    default:
        /* attempt hwaccel - if it fails, then fallback to software decoding */
    case VR_HW_ACCEL_AUTO:
        /* fall through */
    case VR_HW_ACCEL_YES:
        xab_log(LOG_TRACE, "Decoder: attempting HW accel\n");
        xab_log(LOG_TRACE, "Allocating HW context\n");
        dst_dec->hw_ctx = calloc(1, sizeof(DecoderHW_ctx_t));

        xab_log(LOG_TRACE, "Initializing HW accel\n");
        // init hardware accceleration, if it fails, free the HW context thingy
        if (!hw_accel_init(dst_dec->hw_ctx, dst_dec->av_codec)) {
            // TODO: hard error on VR_HW_ACCEL_YES
            xab_log(LOG_INFO, "Decoder: HW accel initialization failed. "
                              "falling back to software decoding\n");
            free(dst_dec->hw_ctx);
            dst_dec->hw_ctx = NULL;
        }
        break;

        /* don't attempt hwaccel - use software decoding */
    case VR_HW_ACCEL_NO:
        dst_dec->hw_ctx = NULL;
        break;
    }

    // -- allocate a codec context and fill it --
    dst_dec->av_codec_ctx = avcodec_alloc_context3(dst_dec->av_codec);
    if (!dst_dec->av_format_ctx) {
        xab_log(LOG_ERROR, "Couldn't allocate AVCodecContext\n");
    }

    if (avcodec_parameters_to_context(dst_dec->av_codec_ctx,
                                      dst_dec->av_codecpar) < 0) {
        xab_log(LOG_ERROR, "Couldn't initialize AVCodecContext\n");
    }

    // more hw accel stuff
    if (dst_dec->hw_ctx) {
        hw_accel_set_avcontext_pixfmt_callback(dst_dec->hw_ctx,
                                               dst_dec->av_codec_ctx);
        // initalize the device thingy
        if (hw_accel_init_device(dst_dec->hw_ctx, dst_dec->av_codec_ctx) < 0) {
            xab_log(LOG_INFO, "Decoder: HW accel initialization failed. "
                              "falling back to software decoding\n");
            free(dst_dec->hw_ctx);
            dst_dec->hw_ctx = NULL;
        }
    }

    // -- open codec --
    if (avcodec_open2(dst_dec->av_codec_ctx, dst_dec->av_codec, NULL) < 0) {
        xab_log(LOG_ERROR, "Couldn't open codec\n");
    }

    // log some stuff
    xab_log(LOG_VERBOSE, "File format long name: %s\n",
            dst_dec->av_format_ctx->iformat->long_name);

    if (dst_dec->hw_ctx != NULL) {
        xab_log(LOG_DEBUG, "Decoder: HW acceleration in use for video decoding "
                           "good luck gpu\n");
    } else {
        xab_log(LOG_DEBUG, "Decoder: no HW acceleration in "
                           "use for video decoding :( good luck "
                           "cpu\n");
    }

    // get time information
    xab_log(LOG_TRACE, "Decoder: Getting time information\n");
    dst_dec->time_base = av_q2d(dst_dec->video->time_base);

    xab_log(LOG_TRACE, "Decoder: Creating threads...\n");
    // initialize mutexes and conds and stuff
    dst_dec->packet_dead = false;
    dst_dec->picture_dead = false;
    pthread_mutex_init(&dst_dec->packet_mutex, NULL);
    pthread_mutex_init(&dst_dec->picture_mutex, NULL);
    pthread_cond_init(&dst_dec->packet_cond, NULL);
    pthread_cond_init(&dst_dec->picture_cond, NULL);

    // block workers from working
    pthread_mutex_lock(&dst_dec->packet_mutex);
    pthread_mutex_lock(&dst_dec->picture_mutex);

    // packet worker
    pthread_create(&dst_dec->packet_worker_tid, NULL, &decoder_packet_worker,
                   dst_dec);

    // picture worker
    pthread_create(&dst_dec->picture_worker_tid, NULL, &decoder_picture_worker,
                   dst_dec);

    // allow workers to start working
    pthread_mutex_unlock(&dst_dec->packet_mutex);
    pthread_mutex_unlock(&dst_dec->picture_mutex);
}

static void *decoder_packet_worker(void *ctx) {
    Decoder_t *dec = (Decoder_t *)ctx;
    AVPacket *packet = av_packet_alloc();
    int response = 0;

    // block here and wait for main thread to finish initialization
    pthread_mutex_lock(&dec->packet_mutex);
    pthread_mutex_unlock(&dec->packet_mutex);

    while (!dec->packet_dead) {
        // enqueue packets
        response = av_read_frame(dec->av_format_ctx, packet);

        if (packet->stream_index != dec->video_stream_idx) {
            av_packet_unref(packet);
            continue;
        }

        // handle looping and read frame errors
        if (response == AVERROR_EOF) {
            xab_log(LOG_TRACE, "Decoder: looping video\n");
            av_seek_frame(dec->av_format_ctx, dec->video_stream_idx,
                          dec->video->start_time != AV_NOPTS_VALUE
                              ? dec->video->start_time
                              : 0,
                          AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(dec->av_codec_ctx);
            av_packet_unref(packet);
            continue;
        } else if (response < 0) {
            xab_log(LOG_ERROR, "Failed to read frame: %s (%d)\n",
                    av_err2str(response), response);
            av_packet_unref(packet);
            continue;
        }

        do {
            if (!dec->packet_dead) {
                pthread_cond_signal(&dec->picture_cond);
                if (!packet_queue_put(&dec->pacq, packet)) {
                    pthread_mutex_lock(&dec->packet_mutex);
                    pthread_cond_wait(
                        &dec->packet_cond,
                        &dec->packet_mutex); // wait for the other thread to get
                                             // some packets from the queue
                    pthread_mutex_unlock(&dec->packet_mutex);
                    continue;
                }
            }
            break;
        } while (true);
        av_packet_unref(packet);
    }

    xab_log(LOG_DEBUG, "Decoder packet thread: Quitting\n");

    av_packet_free(&packet);

    pthread_exit(0);
}

static void *decoder_picture_worker(void *ctx) {
    Decoder_t *dec = (Decoder_t *)ctx;

    // if hardware accceleration is enabled, allocate a software frame
    AVFrame *sw_frame = NULL;
    if (dec->hw_ctx) {
        sw_frame = av_frame_alloc();
        sw_frame->format = AV_PIX_FMT_YUV420P; // assume YUV420
        sw_frame->width = dec->vwidth;
        sw_frame->height = dec->vheight;
        if (av_frame_get_buffer(sw_frame, 32) < 0) {
            av_frame_free(&sw_frame);
            sw_frame = NULL;
        }
    }

    // block here and wait for main thread to finish initialization
    pthread_mutex_lock(&dec->packet_mutex);
    pthread_mutex_unlock(&dec->packet_mutex);

    while (!dec->picture_dead) {
        AVCodecContext *av_codec_ctx = dec->av_codec_ctx;
        const int video_stream_idx = dec->video_stream_idx;
        AVFrame *av_frame = dec->av_frame;
        AVPacket *av_packet = dec->av_packet;

        int response = AVERROR(EINVAL);
        // get a frame (until the frame is finished)
        while ((response = avcodec_receive_frame(av_codec_ctx, av_frame))) {
            if (response == AVERROR_EOF) {
                avcodec_flush_buffers(av_codec_ctx);
                continue;
            }

            do {
                if (!dec->picture_dead) {
                    pthread_cond_signal(&dec->packet_cond);
                    if (!packet_queue_get(&dec->pacq, av_packet)) {
                        pthread_mutex_lock(&dec->picture_mutex);
                        pthread_cond_wait(&dec->picture_cond,
                                          &dec->picture_mutex);
                        pthread_mutex_unlock(&dec->picture_mutex);

                        continue;
                    }
                }
                break;
            } while (true);

            if (av_packet->stream_index != video_stream_idx) {
                av_packet_unref(av_packet);
                continue;
            }

            response = avcodec_send_packet(av_codec_ctx, av_packet);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF ||
                response == AVERROR(EINVAL)) {
                av_packet_unref(av_packet);
                continue;
            } else if (response < 0) {
                av_packet_unref(av_packet);
                xab_log(LOG_ERROR,
                        "Decoder: Failed to decode packet: %s (%d)\n",
                        av_err2str(response), response);
                continue;
            }

            av_packet_unref(av_packet);
        }

        // TODO: instead of transfering the frame back to a sw frame and than
        // uploading it to a texture, just upload the frame to a texture from
        // the gpu directly

        // now that we have the frame, if we have hardware acceleration enabled,
        // and the frame's pixel format matches the hw accel's pixel format then
        // we need to transfer the frame from the gpu to the cpu
        AVFrame *qframe = NULL;
        if (dec->hw_ctx && sw_frame) {
            if (av_frame->format == dec->hw_ctx->hw_pix_fmt) {
                if (av_hwframe_transfer_data(sw_frame, av_frame, 0) < 0) {
                    xab_log(LOG_ERROR, "Decoder: error transferring the data "
                                       "to system memory\n");
                    av_frame_unref(av_frame);
                    continue;
                }
            }
            qframe = sw_frame;
        } else
            qframe = av_frame;
        Assert(qframe != NULL && "Invalid AVFrame* for queueing (NULL)");

        // enqueue the frame
        do {
            if (!dec->picture_dead) {
                pthread_cond_signal(&dec->packet_cond);
                if (!picture_queue_put(&dec->picq, qframe)) {
                    pthread_mutex_lock(&dec->picture_mutex);
                    pthread_cond_wait(
                        &dec->picture_cond,
                        &dec->picture_mutex); // wait for ready back
                    pthread_mutex_unlock(&dec->picture_mutex);
                    continue;
                }
            }
            break;
        } while (true);
    }

    if (sw_frame)
        av_frame_free(&sw_frame);

    xab_log(LOG_DEBUG, "Decoder picture thread: Quitting\n");

    pthread_exit(0);
}

void decoder_decode(Decoder_t *dec) {
    if (!picture_queue_get(&dec->picq, dec->av_pass_frame)) {
        pthread_cond_signal(&dec->picture_cond);
        return;
    }

    if (dec->callback_func)
        (*dec->callback_func)(dec->av_pass_frame, dec->callback_ctx);
}

void decoder_destroy(Decoder_t *dec) {

    // exit packet thread
    pthread_mutex_lock(&dec->packet_mutex);
    dec->packet_dead = true;
    pthread_mutex_unlock(&dec->packet_mutex);
    pthread_cond_broadcast(
        &dec->packet_cond); // broadcast so if the thread is waiting before we
                            // set dead to true, it will be waken up
    // wait for thread to terminate
    pthread_join(dec->packet_worker_tid, NULL);
    pthread_mutex_destroy(&dec->packet_mutex);

    pthread_mutex_lock(&dec->picture_mutex);
    dec->picture_dead = true;
    pthread_mutex_unlock(&dec->picture_mutex);
    pthread_cond_broadcast(
        &dec->picture_cond); // broadcast so if the thread is waiting before we
                             // set dead to true, it will be waken up
    // wait for thread to terminate
    pthread_join(dec->picture_worker_tid, NULL);
    pthread_mutex_destroy(&dec->picture_mutex);

    // destroy the cond
    pthread_cond_destroy(&dec->packet_cond);
    pthread_cond_destroy(&dec->picture_cond);

    // destroy packet queue
    packet_queue_free(&dec->pacq);

    // destroy picture queue
    picture_queue_free(&dec->picq);

    // destroy allocated packets
    if (dec->av_packet)
        av_packet_free(&dec->av_packet);
    if (dec->av_frame)
        av_frame_free(&dec->av_frame);
    if (dec->av_pass_frame)
        av_frame_free(&dec->av_pass_frame);

    // close and free hwaccel
    if (dec->hw_ctx) {
        hw_accel_close(dec->hw_ctx);
        free(dec->hw_ctx);
        dec->hw_ctx = NULL;
    }

    // cleanup ffmpeg things
    if (dec->av_format_ctx) {
        avformat_close_input(&dec->av_format_ctx);
        avformat_free_context(dec->av_format_ctx);
    }
    if (dec->av_codec_ctx)
        avcodec_free_context(&dec->av_codec_ctx);
}
