#include "pch.h"
#include <stdio.h>

#include "context.h"
#include "video_reader_interface.h"
#include "logger.h"
#include "framebuffer.h"
#include "setbg.h"
#include "wallpaper.h"
#include "arg_parser.h"
#include "window.h"
#include "ipc.h"
#include "ipc_spec.h"

// auuugggghh global variables scary
static context_t context;
static IPC_handle_t ipc_handle;
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

    ipc_handle = ipc_init(IPC_PATH);
    context = context_create(opts);

    TracyCZoneEnd(tracy_ctx);
    ON_TRACY(xab_log(LOG_TRACE, "Ending tracy zone `Setup`\n");)
}

static void mainloop(void) {
    xab_log(LOG_DEBUG, "Running main loop...\n");

    ON_TRACY(xab_log(LOG_TRACE, "Starting tracy zone `Mainloop`\n");)
    TracyCZoneNC(tracy_ctx, "Mainloop", TRACY_COLOR_WHITE, true);

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

    xcb_void_cookie_t cookie = xcb_map_window_checked(
        context.xdata.connection, context.xdata.screen->root);
    if ((error = xcb_request_check(context.xdata.connection, cookie)) != NULL) {
        xab_log(LOG_FATAL, "Failed to map the root pixmap\n");
        free(error);
        xcb_disconnect(context.xdata.connection);
        exit(EXIT_FAILURE);
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

        xcb_get_geometry_cookie_t geometry_cookie = xcb_get_geometry(
            context.xdata.connection, context.xdata.screen->root);
        xcb_get_geometry_reply_t *geometry = xcb_get_geometry_reply(
            context.xdata.connection, geometry_cookie, NULL);
        const int width = (int)geometry->width;
        const int height = (int)geometry->height;
        free(geometry);

        TracyCZoneEnd(tracy_ctx2);

        // render only if window size is non-zero (minimized)
        if (width != 0 && height != 0) {
            // TODO: poll every 10 frames with modulo operator
            ipc_poll_events(&ipc_handle, &context);

            // TODO: either control ipc events from the context or create an
            // event manager

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

            // swap the buffers to show output
            if (!eglSwapBuffers(context.display, context.window.surface))
                xab_log(LOG_ERROR, "Failed to swap OpenGL buffers!\n");

            switch (context.window.window_type) {
            case XPIXMAP_BACKGROUND:
                update_background(&context.window.xpixmap, &context.xdata,
                                  context.window.desktop_window);
                break;
            default:
            case XWINDOW_BACKGROUND:
            case XWINDOW:
                break;
            }

            // don't ask
            for (int i = 0; i < context.wallpaper_count; i++)
                report_swap_video(&context.wallpapers[i].video);
        } else {
            // window is minimized, instead sleep a bit
            usleep(10 * 1000);
        }

        TracyCFrameMarkEnd("FrameRender");
    }

    TracyCZoneEnd(tracy_ctx);
    ON_TRACY(xab_log(LOG_TRACE, "Ending tracy zone `Mainloop`\n");)
}

static void cleanup(
    struct argument_options
        *opts) { // maybe i should make the context do some of the cleaning...
    ON_TRACY(xab_log(LOG_TRACE, "Starting tracy zone `Cleanup`\n");)
    TracyCZoneNC(tracy_ctx, "Cleanup", TRACY_COLOR_GREEN, true);

    xab_log(LOG_DEBUG, "Cleaning up...\n");

    context_free(&context);

    // todo: maybe i can free some of the memory earlier
    clean_opts(opts);

    TracyCZoneEnd(tracy_ctx);
    ON_TRACY(xab_log(LOG_TRACE, "Ending tracy zone `Cleanup`\n");)

    printf("xab has shut down gracefully\n");
}

int main(int argc, char *argv[]) {
    ON_TRACY(xab_log(LOG_TRACE, "Starting up the tracy profiler...");)

    struct argument_options opts = parse_args(argc, argv);

    setup(&opts);
    mainloop();
    cleanup(&opts);

    ipc_close(&ipc_handle);

    return EXIT_SUCCESS;
}

static void handle_sigint(int sig) {
    keep_running = false;
    printf("\nCaught signal %d, gracefully shutting down...\n", sig);
}
