#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <epoxy/common.h>

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
#include "setbg.h"
#include "monitor.h"
#include "wallpaper.h"
#include "arg_parser.h"

static context_t context;
static bool keep_running = true;
static void handle_sigint(int sig);

static void setup(struct argument_options *opts) {
    xab_log(LOG_DEBUG, "Initializing...\n");

#ifndef NEDBUG
    xab_log(LOG_DEBUG, "Currently in debug mode\n");
#else
    xab_log(LOG_DEBUG,
            "Currently in release mode\n"); // idk why i did that lol
#endif

    context = context_create(opts);
}

static void mainloop(void) {
    xab_log(LOG_DEBUG, "Runninng main loop...\n");

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
        // delta time
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
    }
}

static void cleanup(
    struct argument_options
        *opts) { // maybe i should make the context do some of the cleaning...
    xab_log(LOG_DEBUG, "Cleaning up...\n");

    // close and clean up the videos
    for (int i = 0; i < context.wallpaper_count; i++)
        wallpaper_close(&context.wallpapers[i]);
    free(context.wallpapers);

    // clean up framebuffer
    delete_framebuffer(&context.framebuffer);

    cleanup_monitors(context.monitor_count, context.monitors);

    // todo: maybe i can free some of the memory earlier
    clean_opts(opts);

    // destroy the EGL context, surface, and display
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
    xcb_free_pixmap(
        context.connection,
        context.background_pixmap); // i'm not sure if im supposed to clean
                                    // this up cuz of the preserve thingy

    // disconnect from the X server
    if (context.connection)
        xcb_disconnect(context.connection);

    printf("xab has shut down gracefully\n");
}

int main(int argc, char *argv[]) {
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
