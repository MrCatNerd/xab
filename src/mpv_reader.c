#include "pch.h"

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include "shader_cache.h"
#include "video_reader_interface.h"
#include "logger.h"
#include "framebuffer.h"

// im too lazy to type 5 extra characters
#define VR_INTERNAL(vrs) ((VRStateInternal_t *)vrs)
typedef struct VRStateInternal {
        mpv_handle *mpv_handle;
        mpv_render_context *mpv_glcontext;
        FrameBuffer_t framebuffer;
        bool redraw_wakeup;
} VRStateInternal_t;

static void *(get_proc_address_mpv)(void *ctx, const char *name);
static void on_mpv_render_update(void *ctx);
static void on_mpv_events(void *ctx);
static void set_init_mpv_options(VideoReaderState_t *state);

VideoReaderState_t open_video(const char *path,
                              VideoReaderRenderConfig_t vr_config,
                              ShaderCache_t *scache) {
    VideoReaderState_t state = {.path = path,
                                .vrc = vr_config,
                                .internal =
                                    calloc(1, sizeof(VRStateInternal_t))};

    VRStateInternal_t *internal_state = VR_INTERNAL(state.internal);

    xab_log(LOG_DEBUG, "Creating video framebuffer\n");
    internal_state->framebuffer = create_framebuffer(
        state.vrc.width * state.vrc.scale, state.vrc.height * state.vrc.scale,
        state.vrc.gl_internal_format, scache);

    xab_log(LOG_DEBUG, "Initializing mpv handle\n");
    internal_state->mpv_handle = mpv_create();
    if (!internal_state->mpv_handle) {
        xab_log(LOG_ERROR, "Failed to create mpv context");
        exit(EXIT_FAILURE);
    }

    set_init_mpv_options(&state);

    int mpv_err = mpv_initialize(internal_state->mpv_handle);
    if (mpv_err < MPV_ERROR_SUCCESS) {
        xab_log(LOG_ERROR, "Failed to initialize mpv: %s",
                mpv_error_string(mpv_err));
        exit(EXIT_FAILURE);
    }

    // run again after mpv_initialize to override options in config files
    set_init_mpv_options(&state);

    // force libmpv vo as nothing else will work
    char *vo_option =
        mpv_get_property_string(internal_state->mpv_handle, "options/vo");
    if (strcmp(vo_option, "libmpv") != 0) {
        if (strcmp(vo_option, "") != 0)
            xab_log(LOG_WARN,
                    "xab does not support any other mpv vo than \"libmpv\"\n");
        mpv_set_option_string(internal_state->mpv_handle, "vo", "libmpv");
    }

    xab_log(LOG_DEBUG, "Initializing mpv render context\n");
    mpv_render_param render_param[] = {
        {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,
         &(mpv_opengl_init_params){get_proc_address_mpv, NULL}},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &(int){1}},
        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &(int){0}},
        {MPV_RENDER_PARAM_INVALID, NULL},
    };

    mpv_err =
        mpv_render_context_create(&internal_state->mpv_glcontext,
                                  internal_state->mpv_handle, render_param);
    if (mpv_err < MPV_ERROR_SUCCESS) {
        xab_log(LOG_ERROR, "Failed to initialize mpv GL context, %s",
                mpv_error_string(mpv_err));
        exit(EXIT_FAILURE);
    }

    xab_log(LOG_DEBUG, "Setting mpv callbacks\n");
    mpv_set_wakeup_callback(internal_state->mpv_handle, on_mpv_events, NULL);
    mpv_render_context_set_update_callback(internal_state->mpv_glcontext,
                                           on_mpv_render_update,
                                           &internal_state->redraw_wakeup);

    xab_log(LOG_DEBUG, "Setting mpv options\n");
#ifndef NDEBUG
    mpv_request_log_messages(internal_state->mpv_handle, "debug");
#else
    mpv_request_log_messages(internal_state->mpv_handle, "no");
#endif

    // load the video file
    const char *cmd[] = {"loadfile", path, NULL};
    mpv_command(internal_state->mpv_handle, cmd);

    // automatically decide whether to use hw decoding or not
    mpv_set_option_string(internal_state->mpv_handle, "hwdec", "auto");

    // gpu api stuff
    mpv_set_option_string(internal_state->mpv_handle, "gpu-context", "opengl");
    mpv_set_option_string(internal_state->mpv_handle, "gpu-api", "opengl");

    // enable / disable hardware acceleration
    {
        // 'auto' is the default
        char *hwdec_opt = "auto";
        switch (vr_config.hw_accel) {
        case VR_HW_ACCEL_NO:
            hwdec_opt = "no";
            break;
        case VR_HW_ACCEL_YES:
            hwdec_opt = "yes";
            break;
        default:
        case VR_HW_ACCEL_AUTO:
            hwdec_opt = "auto";
            break;
        }
        xab_log(LOG_VERBOSE, "Video hardware acceleration: %s\n", hwdec_opt);
        mpv_set_option_string(internal_state->mpv_handle, "hwdec", hwdec_opt);
    }

    // mpv must never idle, or the universe will end
    mpv_command_async(internal_state->mpv_handle, 0,
                      (const char *[]){"set", "idle", "no", NULL});

    return state;
}

static void set_init_mpv_options(VideoReaderState_t *state) {
    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    // point filtering (pixelated video)
    if (state->vrc.pixelated) {
        mpv_set_option_string(internal_state->mpv_handle, "scale", "nearest");
        mpv_set_option_string(internal_state->mpv_handle, "cscale", "nearest");
    }

    // don't use an intermediate buffer to upload from the cpu to the gpu, use
    // direct rendering instead (my cpu is already dead enough)
    mpv_set_option_string(internal_state->mpv_handle, "vd-lavc-dr", "yes");

    // loop the video
    mpv_set_option_string(internal_state->mpv_handle, "loop", "yes");
    // mpv_set_option_string(internal_state->mpv_handle, "loop-playlist",
    // "yes"); // hmmmmmm

    // print annoying stuff to the terminal in debug mode
#ifndef NDEBUG
    mpv_set_option_string(internal_state->mpv_handle, "terminal", "yes");
#else
    // mpv_set_option_string(internal_state->mpv_handle, "terminal",
    //                       "yes"); // todo: change to no when done doing stuff
    mpv_set_option_string(internal_state->mpv_handle, "terminal", "no");
#endif /* ifndef NDEBUG */

    // do not load config from ~/.config/mpv/
    // mpv_set_option_string(
    //     internal_state->mpv_handle, "config",
    //     "no"); // todo: add an option to enable loading the config file

    // don't worry 'bout it
    mpv_set_option_string(internal_state->mpv_handle, "video-timing-offset",
                          "0");
}

void render_video(VideoReaderState_t *state) {
    TracyCZoneNC(tracy_ctx, "VIDEO_RENDER", TRACY_COLOR_GREEN, true);

    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    // if mpv has a new frame, render it
    if (internal_state->redraw_wakeup) {
        internal_state->redraw_wakeup = false;

        if (mpv_render_context_update(internal_state->mpv_glcontext) &
            MPV_RENDER_UPDATE_FRAME) {
            mpv_render_param render_params[] = {
                {MPV_RENDER_PARAM_OPENGL_FBO,
                 &(mpv_opengl_fbo){
                     .fbo = internal_state->framebuffer.fbo_id,
                     .w = internal_state->framebuffer.width * state->vrc.scale,
                     .h = internal_state->framebuffer.height * state->vrc.scale,
                     .internal_format =
                         internal_state->framebuffer.gl_internal_format,
                 }},
                // i don't flip for compatibility with other video readers, the
                // orthographic projection already flips it
                {MPV_RENDER_PARAM_FLIP_Y, &(int){0}},
                // do not wait for a fresh frame to render
                {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &(int){0}},
                {MPV_RENDER_PARAM_INVALID, NULL},
            };

            int mpv_err = mpv_render_context_render(
                internal_state->mpv_glcontext, render_params);
            if (mpv_err < MPV_ERROR_SUCCESS) {
                xab_log(LOG_ERROR, "Failed to render frame with mpv, %s",
                        mpv_error_string(mpv_err));
                exit(EXIT_FAILURE);
            }
        }
    }

    TracyCZoneEnd(tracy_ctx);
}

unsigned int get_video_ogl_texture(VideoReaderState_t *state) {
    return VR_INTERNAL(state->internal)->framebuffer.texture_color_id;
}

void pause_video(VideoReaderState_t *state) {
    mpv_command_async(VR_INTERNAL(state->internal)->mpv_handle, 0,
                      (const char *[]){"set", "pause", "yes", NULL});
}

void unpause_video(VideoReaderState_t *state) {
    mpv_command_async(VR_INTERNAL(state->internal)->mpv_handle, 0,
                      (const char *[]){"set", "pause", "no", NULL});
}

void close_video(VideoReaderState_t *state, ShaderCache_t *scache) {
    xab_log(LOG_VERBOSE, "Closing video: %s\n", state->path);
    if (!state || !state->internal)
        return;

    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);

    // close down + disable callbacks
    if (internal_state->mpv_glcontext) {
        mpv_render_context_set_update_callback(internal_state->mpv_glcontext,
                                               NULL, NULL);
        mpv_render_context_free(internal_state->mpv_glcontext);
    }
    if (internal_state->mpv_handle) {
        mpv_set_wakeup_callback(internal_state->mpv_handle, NULL, NULL);
        mpv_terminate_destroy(internal_state->mpv_handle);
    }

    // delete framebuffer
    delete_framebuffer(&internal_state->framebuffer, scache);

    free(state->internal);
}

void report_swap_video(VideoReaderState_t *state) {
    TracyCZoneNC(tracy_ctx, "ReportSwapVideo", TRACY_COLOR_GREY, true);

    // /* Tell the renderer that a frame was flipped at the given time. This is
    // optional, but can help the player to achieve better timing. Note that
    // calling this at least once informs libmpv that you will use this
    // function. If you use it inconsistently, expect bad video playback. If
    // this is called while no video is initialized, it is ignored. */
    // - from mpv_render_context_report_swap doc
    VRStateInternal_t *internal_state = VR_INTERNAL(state->internal);
    if (internal_state->redraw_wakeup) {
        mpv_render_context_report_swap(
            VR_INTERNAL(state->internal)->mpv_glcontext);
    }

    TracyCZoneEnd(tracy_ctx);
}

static void *(get_proc_address_mpv)(void *ctx, const char *name) {
    (void)ctx;
    return (void *)eglGetProcAddress(name);
}

static void on_mpv_render_update(void *ctx) {
    // we set the wakeup flag here to enable the mpv_render_context_render path
    // in the main loop
    *(bool *)(ctx) = true;
}

static void on_mpv_events(void *ctx) {
    // i will do events later, probably, im not sure if i even need events
    (void)ctx;
    xab_log(LOG_VERBOSE, "%s was called\n", __func__);
}

// dunno why i did that... don't care
#undef VR_INTERNAL
