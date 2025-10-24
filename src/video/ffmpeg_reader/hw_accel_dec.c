#include "hw_accel_dec.h"
#include "logger.h"
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts);

/// av_codec must be initialized before this functions is called
bool hw_accel_init(DecoderHW_ctx_t *dst_hwa_ctx, const AVCodec *av_codec) {
    if (!dst_hwa_ctx)
        return false;

    // get the device type

    // manual
    /* const char *dev_name = "vdpau";
    dst_hwa_ctx->dev_type = av_hwdevice_find_type_by_name(dev_name);
    if (dst_hwa_ctx->dev_type == AV_HWDEVICE_TYPE_NONE) {
        xab_log(LOG_ERROR, "Device type: %s not supported!\n", dev_name);
        return false;
    } */

    // automatic
    // this code is very spaghet
    // clang-format off
    enum AVHWDeviceType prefered[] = { // a switch statement would be more ideal but the sunk cost fallacy has kicked in
        // prefered device type by order (higher=better)
        AV_HWDEVICE_TYPE_VDPAU,
        AV_HWDEVICE_TYPE_CUDA,
        AV_HWDEVICE_TYPE_VAAPI,
        AV_HWDEVICE_TYPE_QSV,
        AV_HWDEVICE_TYPE_VULKAN,
        AV_HWDEVICE_TYPE_OPENCL,
        AV_HWDEVICE_TYPE_MEDIACODEC,
    };
    // clang-format on
    int prefered_size = sizeof(prefered) / sizeof(*prefered);
    int prefered_idx = prefered_size;
    enum AVHWDeviceType device_type = AV_HWDEVICE_TYPE_NONE;
    enum AVHWDeviceType iterated_device_type =
        AV_HWDEVICE_TYPE_NONE; // if there are no avai->sizelable device types
                               // that are prefered, choose the last one

    // iterate through all of the supported device types and choose the last one
    while ((dst_hwa_ctx->dev_type = av_hwdevice_iterate_types(
                dst_hwa_ctx->dev_type)) != AV_HWDEVICE_TYPE_NONE) {
        if (dst_hwa_ctx->dev_type != AV_HWDEVICE_TYPE_NONE) {
            for (int i = 0; i < prefered_size; i++) {
                if (dst_hwa_ctx->dev_type == prefered[i] && i < prefered_idx) {
                    prefered_idx = i;
                    device_type = dst_hwa_ctx->dev_type;
                }
            }
            iterated_device_type = dst_hwa_ctx->dev_type;
        }

        // TODO: log available device types as a list with a | seperator
        xab_log(LOG_TRACE, "Decoder: Available device type: %s\n",
                av_hwdevice_get_type_name(dst_hwa_ctx->dev_type));
    }

    if (prefered_idx < prefered_size)
        device_type = prefered[prefered_idx];
    else
        device_type = iterated_device_type;

    dst_hwa_ctx->dev_type = device_type;
    if (dst_hwa_ctx->dev_type == AV_HWDEVICE_TYPE_NONE) {
        xab_log(LOG_ERROR, "Decoder: No hardware device types available!\n");
        return false;
    }

    // get the hardware config, see if it supports the device type and retrieve
    // the pixel format if it does
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(av_codec, i);
        if (!config) {
            xab_log(LOG_INFO,
                    "Decoder: `%s` decoder does not support device type `%s`\n",
                    av_codec->name,
                    av_hwdevice_get_type_name(dst_hwa_ctx->dev_type));
            return false;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == dst_hwa_ctx->dev_type) {
            xab_log(LOG_INFO,
                    "Decoder: `%s` decoder does support device type `%s`\n",
                    av_codec->name,
                    av_hwdevice_get_type_name(dst_hwa_ctx->dev_type));
            dst_hwa_ctx->hw_pix_fmt = config->pix_fmt;
            break;
        }
    }

    return true;
}

int hw_accel_init_device(DecoderHW_ctx_t *hwa_ctx,
                         AVCodecContext *av_codec_ctx) {
    int error = 0;

    // init device
    if ((error = av_hwdevice_ctx_create(
             &hwa_ctx->hw_device_ctx, hwa_ctx->dev_type, NULL, NULL, 0)) < 0) {
        xab_log(LOG_ERROR, "Decoder: Failed to create specified HW device\n");
        return error;
    }

    av_codec_ctx->hw_device_ctx = av_buffer_ref(hwa_ctx->hw_device_ctx);

    // check if it is possible to convert the GPU pixel format to something
    // valid
    AVHWFramesConstraints *hw_frames_constraints =
        av_hwdevice_get_hwframe_constraints(hwa_ctx->hw_device_ctx, NULL);

    if (!hw_frames_constraints)
        return AVERROR(EINVAL);

    enum AVPixelFormat *pixfmt = (enum AVPixelFormat *)AV_PIX_FMT_NONE;
    for (pixfmt = hw_frames_constraints->valid_sw_formats;
         *pixfmt != AV_PIX_FMT_NONE; pixfmt++) {
        if (*pixfmt == AV_PIX_FMT_YUV420P)
            break;
    }
    av_hwframe_constraints_free(&hw_frames_constraints);

    // if the GPU is unable to convert to YUV420p then give up and let down
    if (*pixfmt == AV_PIX_FMT_NONE) {
        xab_log(LOG_ERROR,
                "Decoder: your device is unable to convert to YUV420p :(\n");
        return AVERROR(EINVAL);
    }

    return error;
}

void hw_accel_set_avcontext_pixfmt_callback(DecoderHW_ctx_t *hwa_ctx,
                                            AVCodecContext *av_codec_ctx) {
    av_codec_ctx->opaque = &hwa_ctx->hw_pix_fmt;
    av_codec_ctx->get_format = get_hw_format;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts) {
    // i set the pixel format in the opaque thingy
    const enum AVPixelFormat hw_pix_fmt =
        (const enum AVPixelFormat) * (const enum AVPixelFormat *)ctx->opaque;
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    xab_log(LOG_ERROR, "Decoder: Failed to get HW surface format\n");
    return AV_PIX_FMT_NONE;
}

void hw_accel_close(DecoderHW_ctx_t *hwa_ctx) {
    av_buffer_unref(&hwa_ctx->hw_device_ctx);
}
