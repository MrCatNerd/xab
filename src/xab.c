#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>
#include <xcb/randr.h>

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <epoxy/common.h>
#include <epoxy/gl_generated.h>
#include <epoxy/egl_generated.h>

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
#include "video_renderer.h"
#include "logging.h"
#include "framebuffer.h"
#include "utils.h"
#include "setbg.h"
#include "egl_stuff.h"

static context_t context;

static bool keep_running = true;
static void handle_sigint(int sig);

struct argument_options {
        char *video_path;
        int screen_nbr;
        bool vsync;
        int max_framerate;
        bool pixelated;
        bool hw_accel;
};

static void setup(struct argument_options *opts) {
    LOG("-- Initializing...\n");

#ifndef NEDBUG
    LOG("-- Currently in debug mode\n");
#else
    LOG("-- Currently in release mode\n"); // idk why i did that lol
#endif
    context = context_create(true);

    // set vsync
    EGLBoolean ok = eglSwapInterval(context.display, (int)opts->vsync);
    Assert(ok && "Failed to set VSync for EGL");

    // load the video
    VideoRendererConfig_t config = {.pixelated = opts->pixelated,
                                    .hw_accel = opts->hw_accel};
    context.video = video_from_file(opts->video_path, config);

    // sync everything up cuz why not
    xcb_aux_sync(context.connection);
}

static void mainloop() {
    LOG("-- Runninng main loop...\n");

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
    float angle = 0.0f;
    float shear_thingy = 0.0f;

    while (true) {
        // get size
        xcb_get_geometry_cookie_t geom_cookie;
        xcb_get_geometry_reply_t *geometry;

        geom_cookie =
            xcb_get_geometry(context.connection, context.screen->root);
        geometry =
            xcb_get_geometry_reply(context.connection, geom_cookie, NULL);
        const int width = (int)geometry->width;
        const int height = (int)geometry->height;

        // delta time
        struct timespec c2;
        clock_gettime(CLOCK_MONOTONIC, &c2);
        float delta =
            (float)(c2.tv_sec - c1.tv_sec) + 1e-9f * (c2.tv_nsec - c1.tv_nsec);
        c1 = c2;

        // render only if window size is non-zero (minimized)
        if (width != 0 && height != 0) {
            // setup output size covering all client area of window
            glViewport(0, 0, width, height);

            // clear screen
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                    GL_STENCIL_BUFFER_BIT);
            // render_framebuffer_start_render(&context.framebuffer);
            // glViewport(0, 0, width, height);

            // -- uniform and texture stuff
            // setup rotation matrix in uniform
            {
                da_time += delta;

                // angle += delta * 2.0f * (float)M_PI /
                //          20.0f; // full rotation in 20 seconds
                // angle = fmodf(angle, 2.0f * (float)M_PI);

                float aspect = (float)height / width;

                // clang-format off
                float matrix[] = {
                    cosf(angle) * aspect, -sinf(angle), 0.0f,
                    sinf(angle) * aspect, cosf(angle) , 0.0f,
                    1.0f                ,        0.0f , 0.0f,
                };

                // shear_thingy = sin((fmodf(da_time, (float)(M_PI) * 2) - (float)(M_PI) * 2));
                const double speed = 0.2;
                shear_thingy = sin(da_time * speed);

                float shear[] =  {
                    shear_thingy, 0.0f, 0.0f,
                    0.0f               , 1.0f, 0.0f,
                    0.0f               , 0.0f, 1.0f,
                };

                // set uniforms
                glUseProgram(context.video.shader_program);
                GLCALL(glUniformMatrix3fv(glGetUniformLocation(
                                       context.video.shader_program, "rot_mat"),
                                   1, GL_FALSE, matrix));

                GLCALL(glUniformMatrix3fv(
                    glGetUniformLocation(context.video.shader_program,
                                         "shear_mat"),
                    1, GL_FALSE, shear));
            }

            video_render(&context.video);

            // render_framebuffer_end_render(&context.framebuffer);

            // swap the buffers to show output
            if (!eglSwapBuffers(context.display, context.surface)) {
                program_error("Failed to swap OpenGL buffers!\n");
            }

            update_background(&context);
        } else {
            // window is minimized, instead sleep a bit
            usleep(10 * 1000);
        }
        if (!keep_running)
            break;
    }
}

static void
cleanup() { // maybe i should make the context do some of the cleaning
    LOG("-- Cleaning up...\n");

    // close and clean up video
    video_clean(&context.video);

    // clean up framebuffer
    delete_framebuffer(&context.framebuffer);

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
        context
            .background_pixmap); // i'm not sure if im supposed to clean this up
                                 // cuz of the preserve thingy but im not sure

    // disconnect from the XCB server
    if (context.connection) {
        xcb_disconnect(context.connection);
    }

    LOG("-- Cleanup complete.\n");
}

static struct argument_options parse_args(int argc, char *argv[]) {
    if (argc < 2 || argc > 5) {
        fprintf(stderr,
                "Usage: %s <video> <screen_number> "
                "[max_framerate=0|n] [pixelated=0|1] [hw_accel=0|1] "
                "[vsync=0|1]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    // -- required:
    char *video_file = argv[1];
    int screen_nbr = atoi(argv[2]);

    if (screen_nbr <= 0) {
        program_error("Invalid screen number!\n");
        exit(1);
    }

    // -- optionals:

    // defaults:
    bool vsync = true;
    bool pixelated = false;
    bool hw_accel = true;
    int max_framerate = 0;

    for (int i = 3; i < argc; i++) {
        char *arg = argv[i];

        char *key = strtok(arg, "=");
        char *value = strtok(arg, "=");

        if (key == NULL || value == NULL)
            continue;

        if (strcmp(key, "--pixelated") == 0)
            pixelated = atoi(value) >= 1;
        else if (strcmp(key, "--hw_accel") == 0)
            hw_accel = atoi(value) >= 1;
        else if (strcmp(key, "--vsync") == 0)
            vsync = atoi(value) >= 1;
        else if (strcmp(key, "--max_framerate") == 0) // TODO:: implement this
            max_framerate = atoi(value);
    }

    struct argument_options opts = {.video_path = video_file,
                                    .screen_nbr = screen_nbr,
                                    .vsync = vsync,
                                    .max_framerate = max_framerate,
                                    .pixelated = pixelated,
                                    .hw_accel=hw_accel};

    return opts;
}

int main(int argc, char *argv[]) {
    struct argument_options opts = parse_args(argc, argv);

    setup(&opts);
    mainloop();
    cleanup();

    return EXIT_SUCCESS;
}

static void handle_sigint(int sig) {
    keep_running = false;
    printf("\nCaught signal %d, gracefully shutting down...\n", sig);
}
