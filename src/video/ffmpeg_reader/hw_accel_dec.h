#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/buffer.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavutil/pixfmt.h>

typedef struct DecoderHW_ctx {
        AVBufferRef *hw_device_ctx;
        enum AVHWDeviceType dev_type;
        enum AVPixelFormat hw_pix_fmt;
} DecoderHW_ctx_t;

bool hw_accel_init(DecoderHW_ctx_t *dst_hwa_ctx, const AVCodec *av_codec);
/// sets the pixfmt callback for av_codec_ctx, also this sets the opaque member
/// to the hardware pixel format
void hw_accel_set_avcontext_pixfmt_callback(DecoderHW_ctx_t *hwa_ctx,
                                            AVCodecContext *av_codec_ctx);

/// returns 0 on success
int hw_accel_init_device(DecoderHW_ctx_t *hwa_ctx,
                         AVCodecContext *av_codec_ctx);

void hw_accel_close(DecoderHW_ctx_t *hwa_ctx);
