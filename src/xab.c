#include "pch.h"
#include <stdio.h>

// libepoxy havn't updated their khoronos registry yet, there is an unmerged
// pull request about it and I got nothing to about it until it gets merged :(
#ifndef EGL_EXT_platform_xcb
#define EGL_EXT_platform_xcb 1
#endif /* EGL_EXT_platform_xcb */
#ifndef EGL_PLATFORM_XCB_EXT
#define EGL_PLATFORM_XCB_EXT 0x31DC
#endif /* EGL_PLATFORM_XCB_EXT */
#ifndef EGL_PLATFORM_XCB_SCREEN_EXT
#define EGL_PLATFORM_XCB_SCREEN_EXT 0x31DE
#endif /* EGL_PLATFORM_XCB_SCREEN_EXT */

#include "context.h"
#include "video_reader_interface.h"
#include "logger.h"
#include "framebuffer.h"
#include "shader_cache.h"
#include "setbg.h"
#include "monitor.h"
#include "wallpaper.h"
#include "arg_parser.h"

static context_t context;
static bool keep_running = true;
static void handle_sigint(int sig);

static void setup(struct argument_options *opts) {
    ON_TRACY(xab_log(LOG_TRACE, "Starting tracy zone `Setup`\n");)
    TracyCZoneNC(tracy_ctx, "Setup", TRACY_COLOR_GREEN, true);

    xab_log(LOG_DEBUG, "Initializing...\n");

#ifndef NEDBUG
    xab_log(LOG_DEBUG, "Currently in debug mode\n");
#else
    xab_log(LOG_DEBUG,
            "Currently in release mode\n"); // idk why i did that lol
#endif

    context = context_create(opts);

    ON_TRACY(xab_log(LOG_TRACE, "Ending tracy zone `Setup`\n");)
    TracyCZoneEnd(tracy_ctx);
}

static void mainloop(void) {
    xab_log(LOG_DEBUG, "Runninng main loop...\n");

    ON_TRACY(xab_log(LOG_TRACE, "Starting tracy zone `Mainloop`\n");)
    TracyCZoneNC(tracy_ctx, "Mainloop", TRACY_COLOR_WHITE, true);
    printf("%d", LOG_LEVEL);

    // set up the signal handler (so the program would gracefully exit on
    // Ctrl+c)
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask); // no additional signals blocked during handler
    sigaction(SIGINT, &sa, NULL);

    // enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // disble depth testing
    glDisable(GL_DEPTH_TEST);

    // disable culling
    glDisable(GL_CULL_FACE);

    xcb_generic_error_t *error;

    xcb_void_cookie_t cookie =
        xcb_map_window_checked(context.connection, context.screen->root);
    if ((error = xcb_request_check(context.connection, cookie)) != NULL) {
        free(error);
        exit(-1);
    }

    struct timespec c1;
    clock_gettime(CLOCK_MONOTONIC, &c1);

    float da_time = 0.0f;

    while (keep_running) {
        TracyCFrameMarkStart("FrameRender");

        // delta time
        TracyCZoneNC(tracy_ctx2, "Stuff", TRACY_COLOR_BLUE, true);

        struct timespec c2;
        clock_gettime(CLOCK_MONOTONIC, &c2);
        float delta =
            (float)(c2.tv_sec - c1.tv_sec) + 1e-9f * (c2.tv_nsec - c1.tv_nsec);
        c1 = c2;
        da_time += delta;

        xcb_get_geometry_cookie_t geometry_cookie =
            xcb_get_geometry(context.connection, context.screen->root);
        xcb_get_geometry_reply_t *geometry =
            xcb_get_geometry_reply(context.connection, geometry_cookie, NULL);
        const int width = (int)geometry->width;
        const int height = (int)geometry->height;

        TracyCZoneEnd(tracy_ctx2);

        // render only if window size is non-zero (minimized)
        if (width != 0 && height != 0) {
            // setup output size covering all client area of window
            glViewport(0, 0, width,
                       height); // we have to set the Viewport on every cycle

            // clear screen
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                    GL_STENCIL_BUFFER_BIT);

            // framebuffer start
            render_framebuffer_start_render(&context.framebuffer);

            // render video/s to framebuffer
            for (int i = 0; i < context.wallpaper_count; i++)
                wallpaper_render(&context.wallpapers[i], &context.camera,
                                 &context.framebuffer);

            camera_reset_gl_viewport(
                &context.camera); // we have to set the viewport cuz the
                                  // video renderer will change it

            // framebuffer end
            render_framebuffer_end_render(&context.framebuffer, 0, da_time);

            // don't ask
            for (int i = 0; i < context.wallpaper_count; i++)
                report_swap_video(&context.wallpapers[i].video);

            // swap the buffers to show output
            if (!eglSwapBuffers(context.display, context.surface))
                xab_log(LOG_ERROR, "Failed to swap OpenGL buffers!\n");

            update_background(&context);
        } else {
            // window is minimized, instead sleep a bit
            usleep(10 * 1000);
        }

        TracyCFrameMarkEnd("FrameRender");
    }

    ON_TRACY(xab_log(LOG_TRACE, "Ending tracy zone `Mainloop`\n");)
    TracyCZoneEnd(tracy_ctx);
}

static void cleanup(
    struct argument_options
        *opts) { // maybe i should make the context do some of the cleaning...
    ON_TRACY(xab_log(LOG_TRACE, "Starting tracy zone `Cleanup`\n");)
    TracyCZoneNC(tracy_ctx, "Cleanup", TRACY_COLOR_GREEN, true);

    xab_log(LOG_DEBUG, "Cleaning up...\n");

    // close and clean up the videos
    for (int i = 0; i < context.wallpaper_count; i++)
        wallpaper_close(&context.wallpapers[i], &context.scache);
    free(context.wallpapers);

    // clean up framebuffer
    delete_framebuffer(&context.framebuffer, &context.scache);

    cleanup_monitors(context.monitor_count, context.monitors);

    // clean up shader cache
    shader_cache_cleanup(&context.scache);

    // todo: maybe i can free some of the memory earlier
    clean_opts(opts);

    // destroy the EGL context, surface, and display
    xab_log(LOG_DEBUG, "Cleaning up EGL stuff\n");
    if (context.context != EGL_NO_CONTEXT) {
        eglMakeCurrent(context.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        eglDestroyContext(context.display, context.context);
    }
    if (context.surface != EGL_NO_SURFACE) {
        eglDestroySurface(context.display, context.surface);
    }
    if (context.display != EGL_NO_DISPLAY) {
        eglTerminate(context.display);
    }

    // clean up background pixmap
    xab_log(LOG_DEBUG, "Freeing the background pixmap\n");
    xcb_free_pixmap(
        context.connection,
        context.background_pixmap); // i'm not sure if im supposed to clean
                                    // this up cuz of the preserve thingy

    // disconnect from the X server
    xab_log(LOG_DEBUG, "Disconnecting from the X server...\n");
    if (context.connection)
        xcb_disconnect(context.connection);

    ON_TRACY(xab_log(LOG_TRACE, "Ending tracy zone `Cleanup`\n");)
    TracyCZoneEnd(tracy_ctx);

    printf("xab has shut down gracefully\n");
}

int main(int argc, char *argv[]) {
    ON_TRACY(xab_log(LOG_TRACE, "Starting up the tracy profiler...");)

    struct argument_options opts = parse_args(argc, argv);

    setup(&opts);
    mainloop();
    cleanup(&opts);

    return EXIT_SUCCESS;
}

static void handle_sigint(int sig) {
    keep_running = false;
    printf("\nCaught signal %d, gracefully shutting down...\n", sig);
}
