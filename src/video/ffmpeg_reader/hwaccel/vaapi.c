#include "logger.h"

#include "hwaccels_apis.h"
#include "utils.h"

#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <va/va.h>

int exit_vaapi(HwaAPIHandle handle);

typedef struct VaapiSharedHandle {
        _Atomic int refs;
} VaapiSharedHandle_t;

typedef struct VaapiHandle {
        VaapiSharedHandle_t *shandle;

        AVBufferRef *av_device_ref;

        // vaapi stuff
        unsigned int picture_width;
        unsigned int picture_height;
} VaapiHandle_t;

static VaapiSharedHandle_t *RefSharedHandle(void) {
    static VaapiSharedHandle_t shandle = {0};
    shandle.refs++;
    return &shandle;
}

static void UnrefSharedHandle(VaapiSharedHandle_t *shandle) {
    if (!shandle)
        return;

    if (--shandle->refs <= 0) {
        xab_log(LOG_VERBOSE,
                "Shared module has %d refs left, uninitializing...\n",
                shandle->refs);
    }
}

static int initSharedHandle(void) {
    static _Atomic bool initialized = false;
    if (initialized)
        return 0;

    xab_log(LOG_VERBOSE, "Initalizing VAAPI shared handle...\n");
    VaapiSharedHandle_t *shandle = RefSharedHandle();

    // abuse ref system
    shandle->refs--;

    initialized = true;
    xab_log(LOG_VERBOSE, "VAAPI shared handle is initialized!\n");
    return 0;
}

int init_vaapi(HwaAPIHandle *handle) {
    if (!handle)
        return -1;

    {
        const int status = initSharedHandle();
        if (status < 0)
            return status;
    }

    // allocate memory
    VaapiHandle_t *vaapi_handle = calloc(1, sizeof(VaapiHandle_t));
    if (!vaapi_handle)
        return -1;
    vaapi_handle->shandle = RefSharedHandle();
    *handle = vaapi_handle;

    // set every vaapi thingy to uninitialized
    vaapi_handle->picture_width = 0;
    vaapi_handle->picture_height = 0;

    // create video output surface

    return 0;
}

static void free_device_ref(struct AVHWDeviceContext *hwctx) {
    HwaAPIHandle *handle = (void *)hwctx->user_opaque;
    if (!handle) {
        xab_log(LOG_ERROR, "VAAPI free device ref user_opaque is NULL!\n");
        return;
    }
    if (exit_vaapi(handle) < 0) {
        xab_log(LOG_WARN, "exit_vaapi failed!\n");
    }
}

create_device_vaapi_t create_device_vaapi(HwaAPIHandle handle) {
    xab_log(LOG_VERBOSE, "Creating VAAPI AVHWDeviceContext\n");
    create_device_vaapi_t ret = {
        .err = -1,
        .avref = NULL,
    };

    VaapiHandle_t *vaapi_handle = handle;

    xab_log(LOG_TRACE,
            "Allocating AVHWDeviceContext via av_hwdevice_ctx_alloc\n");
    AVBufferRef *avref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
    if (!avref) {
        xab_log(LOG_ERROR, "Failed to allocate VAAPI AVHWDeviceContext!\n");
        return ret;
    }

    vaapi_handle->av_device_ref = avref;

    xab_log(LOG_TRACE, "Setting AVHWDeviceContext's free device callback\n");
    AVHWDeviceContext *hwctx = (void *)avref->data;
    hwctx->free = free_device_ref;
    hwctx->user_opaque = handle;

    xab_log(LOG_TRACE, "Setting AVHWDeviceContext's VAAPI context\n");
    AVVAAPIDeviceContext *vaapictx = hwctx->hwctx;
    if (!vaapictx) {
        xab_log(LOG_ERROR, "AVHWDeviceContext's VAAPI context is NULL!\n");
        return ret;
    }

    xab_log(LOG_TRACE,
            "Initializing AVHWDeviceContext via av_hwdevice_ctx_init\n");
    if (av_hwdevice_ctx_init(avref) < 0) {
        xab_log(LOG_ERROR, "Failed to initialize AVHWDeviceContext!\n");
        return ret;
    }

    ret.avref = avref;
    ret.err = 0;
    xab_log(LOG_DEBUG, "VAAPI device created successfully!\n");
    return ret;
}

int exit_vaapi(HwaAPIHandle handle) {
    if (!handle)
        return -1;
    xab_log(LOG_VERBOSE, "Exiting a VAAPI hwaccel instance...\n");

    VaapiHandle_t *vaapi_handle = handle;
    Assert(vaapi_handle != NULL && "Invalid vaapi handle!");

    if (vaapi_handle->av_device_ref) {
        xab_log(LOG_TRACE, "Unrefing VAAPI device ref\n");
        av_buffer_unref(&vaapi_handle->av_device_ref);
    }

    xab_log(LOG_TRACE, "Unrefing VPDAU shared handle\n");
    UnrefSharedHandle(vaapi_handle->shandle);

    xab_log(LOG_TRACE, "Freeing the VAAPI hwaccel instance's handle\n");
    free(vaapi_handle);

    return 0;
}
