#include "ffmpeg_reader/decoder.h"
#include "ffmpeg_reader/packet_queue.h"

Decoder_t decoder_init(const char *path, unsigned int width,
                       unsigned int height, bool pixelated,
                       void (*callback_func)(AVFrame *frame,
                                             void *callback_ctx),
                       void *callback_ctx) {
    // -- initalize struct --
    Decoder_t dec = {
        .pq = packet_queue_init(70),
        .vwidth = 0,
        .vheight = 0,

        .av_format_ctx = NULL,
        .av_codec = NULL,
        .av_codec_ctx = NULL,
        .sws_scaler_ctx = NULL,
        .video_stream_idx = -1,

        .video = NULL,
        .av_frame = av_frame_alloc(),
        .raw_av_frame = av_frame_alloc(),
        .av_packet = av_packet_alloc(),
        .frame_size_bytes = 0,

        .start_time = 0,
        .pt_sec = 0,
        .time_base = {.num = 0, .den = 1},

        .callback_func = callback_func,
        .callback_ctx = callback_ctx,
    };

    // -- allocate memeory for the scaled frame
    {
        enum AVPixelFormat pix_fmt = AV_PIX_FMT_RGB24;
        if (!dec.raw_av_frame) {
            xab_log(LOG_ERROR, "Failed to allocate AVFrame: raw_av_frame\n");
        }
        dec.raw_av_frame->width = width;
        dec.raw_av_frame->height = height;
        dec.raw_av_frame->format = pix_fmt;

        // allocate the frame buffer

        if (av_frame_get_buffer(dec.raw_av_frame, 32) // 32-byte alignment
        ) {
            xab_log(LOG_ERROR, "Failed to allocate frame buffer\n");
        }
    }

    // -- initalize av_format_ctx --
    dec.av_format_ctx = avformat_alloc_context();
    if (dec.av_format_ctx == NULL) {
        xab_log(LOG_ERROR, "Couldn't create AVFormatContext\n");
        // haha no error handling deal with it
    }

    if (avformat_open_input(&dec.av_format_ctx, path, NULL, NULL) != 0) {
        xab_log(LOG_ERROR, "Couldn't open video file: %s\n", path);
    }

    avformat_find_stream_info(dec.av_format_ctx, NULL);

    xab_log(LOG_VERBOSE, "File format long name: %s\n",
            dec.av_format_ctx->iformat->long_name);

    // -- codec params stuff --
    AVCodecParameters *av_codec_params = NULL;
    AVCodec *av_codec = NULL;
    dec.video_stream_idx = -1;

    // find the first valid video stream inside the file
    for (unsigned int i = 0; i < dec.av_format_ctx->nb_streams; ++i) {
        AVStream *stream = dec.av_format_ctx->streams[i];

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
            dec.video_stream_idx = i;
            dec.vwidth = av_codec_params->width;
            dec.vheight = av_codec_params->height;
            dec.time_base = stream->time_base;
            dec.av_codec = av_codec; // TODO: check if i need this
            break;
        }
    }

    if (dec.video_stream_idx == -1) {
        xab_log(LOG_ERROR,
                "Couldn't find a valid video stream inside file: %s\n", path);
    }

    // -- setup a codec context --
    dec.av_codec_ctx = avcodec_alloc_context3(av_codec);
    if (!dec.av_format_ctx) {
        xab_log(LOG_ERROR, "Couldn't allocate AVCodecContext\n");
    }

    if (avcodec_parameters_to_context(dec.av_codec_ctx, av_codec_params) < 0) {
        xab_log(LOG_ERROR, "Couldn't initialize AVCodecContext\n");
    }
    if (avcodec_open2(dec.av_codec_ctx, av_codec, NULL) < 0) {
        xab_log(LOG_ERROR, "Couldn't open codec\n");
    }

    // -- log hw_accel (even though not implented yet)
    if (dec.av_codec_ctx->hwaccel != NULL) {
        xab_log(LOG_VERBOSE,
                "HW acceleration in use for video "
                "reading: %s\n",
                dec.av_codec_ctx->hwaccel->name);
    } else {
        xab_log(LOG_VERBOSE, "no HW acceleration in "
                             "use for video decoding :( good luck "
                             "cpu\n");
    }

    // -- create sws_scaler_ctx --
    // for pixelated option
    enum SwsFlags flags = -1;
    if (pixelated)
        flags = SWS_POINT;
    else
        flags = SWS_FAST_BILINEAR;
    Assert((int)flags != -1 && "Program is stoooopid");

    dec.sws_scaler_ctx = sws_getContext(
        dec.vwidth, dec.vheight, dec.av_codec_ctx->pix_fmt, dec.vwidth,
        dec.vheight, AV_PIX_FMT_RGB24, flags, NULL, NULL, NULL);

    if (!dec.sws_scaler_ctx) {
        xab_log(LOG_ERROR, "Failed to create SwsContext scaler");
    }

    // -- get stats --
    dec.start_time =
        dec.av_format_ctx->streams[dec.video_stream_idx]->start_time;

    dec.frame_size_bytes = dec.vwidth * dec.vheight * 3;
    xab_log(LOG_DEBUG, "Frame size %zuMB\n", dec.frame_size_bytes / 1048576);

    return dec;
}

void decoder_decode(Decoder_t *dec) {
    AVFormatContext *av_format_ctx = dec->av_format_ctx;
    AVCodecContext *av_codec_ctx = dec->av_codec_ctx;
    int video_stream_idx = dec->video_stream_idx;
    AVFrame *av_frame = dec->av_frame;
    AVFrame *raw_av_frame = dec->raw_av_frame;
    AVPacket *av_packet = dec->av_packet;

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

    if (sws_scale(dec->sws_scaler_ctx, (const uint8_t *const *)av_frame->data,
                  av_frame->linesize, 0, av_frame->height, raw_av_frame->data,
                  raw_av_frame->linesize) <= 0) {
        xab_log(LOG_ERROR, "Failed to scale YUV to RGB");
        return;
    }

    (*dec->callback_func)(raw_av_frame, dec->callback_ctx);

    // get the time in seconds
    // dec->pt_sec = av_frame->pts * av_q2d(internal_state->time_base);

    // xab_log(LOG_VERBOSE, "video time: %f | real time: %f | %ld/%ld\n",
    //         dec->pt_sec, get_time_since_start(), av_frame->pts,
    //         av_format_ctx->streams[video_stream_idx]->duration);

    // TODO: looping
    /* if (av_frame->pts == last_pts) {
        xab_log(LOG_VERBOSE, "looping video\n");
        const int64_t start_time =
            (int64_t)(dec->internal_data[0] & 0xFFFFFFFFFFFFFFFF);
        av_seek_frame(av_format_ctx, video_stream_idx,
                      start_time != AV_NOPTS_VALUE ? start_time : 0,
                      AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(av_codec_ctx);
    } */
}

void decoder_destroy(Decoder_t *dec) {
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
