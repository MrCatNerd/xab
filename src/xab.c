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

/* #define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <EGL/eglplatform.h> */
#include <epoxy/common.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <epoxy/gl_generated.h>
#include <epoxy/egl_generated.h>

// libepoxy havn't updated their khoronos registry yet, there is an unmerged
// pull request about it and I got nothing to about it until it gets merged :(
#ifndef EGL_EXT_platform_xcb
#define EGL_EXT_platform_xcb 1
#define EGL_PLATFORM_XCB_EXT 0x31DC
#define EGL_PLATFORM_XCB_SCREEN_EXT 0x31DE
#endif /* EGL_EXT_platform_xcb */

#include "logging.h"
#include "egl_stuff.h"
#include "egl_test.h"
#include "utils.h"
#include "context.h"
#include "video_renderer.h"

static Context_t context;

static bool keep_running = true;
static void handle_sigint(int sig);

static void program_error(const char *format, ...);

struct argument_options {
        char *video_path;
        int screen_nbr;
        bool vsync;
        int forced_framerate;
};

static void setup(struct argument_options *opts) {
    LOG("-- Initializing...\n");

#ifndef NEDBUG
    LOG("-- Currently in debug mode\n");
#else
    LOG("-- Currently in release mode\n"); // idk why i did that lol
#endif

    LOG("-- Checking egl extensions...\n");
    if (!eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS) ||
        !strstr(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS),
                "EGL_EXT_platform_xcb")) {
        program_error("-- ERROR: EGL_EXT_platform_xcb is not supported.");
        exit(EXIT_FAILURE);
    } else
        LOG("-- EGL_EXT_platform_xcb supported.\n");

    LOG("-- Connecting to the xcb server\n");
    // open the connection to the X server
    context.connection = xcb_connect(NULL, &context.screen_nbr);

    if (context.connection == NULL ||
        xcb_connection_has_error(context.connection))
        program_error("Error opening display.\n");

    Assert(0 >= context.screen_nbr && "Invalid screen number.");

    LOG("-- Getting the first screen\n");
    // get the first screen
    context.screen =
        xcb_setup_roots_iterator(xcb_get_setup(context.connection)).data;

    if (context.screen == NULL)
        program_error("Unable to get the screen.\n");

    VLOG("\n");
    VLOG("Information of screen %" PRIu32 ":\n", context.screen->root);
    VLOG("  screen number.: %u\n", context.screen_nbr);
    VLOG("  width.........: %" PRIu16 "px\n", context.screen->width_in_pixels);
    VLOG("  height........: %" PRIu16 "px\n", context.screen->height_in_pixels);
    VLOG("  depth.........: %" PRIu8 " bits\n", context.screen->root_depth);
    VLOG("  white pixel...: 0x%08x\n", context.screen->white_pixel);
    VLOG("  black pixel...: 0x%08x\n", context.screen->black_pixel);
    VLOG("\n");

    // initialize EGL
    context.display =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_XCB_EXT, context.connection,
                                 (const EGLint[]){
                                     EGL_PLATFORM_XCB_SCREEN_EXT,
                                     context.screen_nbr,
                                     EGL_NONE,
                                 });

    Assert(context.display != EGL_NO_DISPLAY && "Failed to get EGL display.");

    {

        EGLint major, minor;
        if (!eglInitialize(context.display, &major, &minor)) {
            program_error("Cannot initialize EGL display.");
        }
        if (major < 1 || minor < 5) {
            program_error("EGL version 1.5 or higher required. you are "
                          "currently on version: %d.%d",
                          major, minor);
        } else
            LOG("-- EGL version %d.%d\n", major, minor);

        LOG("-- libepoxy EGL version: %0.1f\n",
            (float)epoxy_egl_version(context.display) / 10);

        // don't assert on debug mode
#ifndef NDEBUG
        pretty_print_egl_check(false, "-- eglInitialize call");
#else
        pretty_print_egl_check(true, "-- eglInitialize call");
#endif
    }

    // TODO: this

    // // plan: create new pixmap, set xroot_pmap_id to be the new pixmap, W
    // xcb_change_window_attributes(c, context.screen->root, XCB_CW_BACK_PIXMAP,
    // &p); xcb_clear_area(c, 0, context.screen->root, 0, 0, 0, 0);

    // choose OpenGL API for EGL, by default it uses OpenGL ES
    EGLBoolean ok = eglBindAPI(EGL_OPENGL_API);
    Assert(ok == EGL_TRUE && "Failed to select OpenGL API for EGL");

    // get the background window id
    {
        xcb_intern_atom_cookie_t xroot_cookie =
            xcb_intern_atom(context.connection, false, strlen("_XROOTPMAP_ID"),
                            "_XROOTPMAP_ID");
        xcb_intern_atom_cookie_t esetroot_cookie =
            xcb_intern_atom(context.connection, false,
                            strlen("ESETROOT_PMAP_ID"), "ESETROOT_PMAP_ID");

        xcb_intern_atom_reply_t *xroot_reply =
            xcb_intern_atom_reply(context.connection, xroot_cookie, NULL);

        xcb_intern_atom_reply_t *esetroot_reply =
            xcb_intern_atom_reply(context.connection, esetroot_cookie, NULL);

        Assert(xroot_reply != NULL && "Failed to get the _XROOTPMAP_ID atom");
        Assert(esetroot_reply != NULL &&
               "Failed to get the ESETROOT_PMAP_ID atom");

        xcb_get_property_reply_t *xroot_value = xcb_get_property_reply(
            context.connection,
            xcb_get_property(context.connection, 0, context.screen->root,
                             xroot_reply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0,
                             ~0),
            NULL);

        xcb_get_property_reply_t *esetroot_value = xcb_get_property_reply(
            context.connection,
            xcb_get_property(context.connection, 0, context.screen->root,
                             esetroot_reply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0,
                             ~0),
            NULL);

        Assert(xroot_value != NULL && "Failed to get the _XROOTPMAP_ID atom");
        Assert(esetroot_value != NULL &&
               "Failed to get the ESETROOT_PMAP_ID atom");

        context.xroot_window =
            (xcb_window_t *)xcb_get_property_value(xroot_value);
        context.esetroot_window =
            (xcb_window_t *)xcb_get_property_value(esetroot_value);

        free(xroot_value);
        free(xroot_reply);
        free(esetroot_value);
        free(esetroot_reply);
    }

    LOG("-- _XROOTPMAP_ID....: 0x%" PRIx32 "\n", *context.xroot_window);
    LOG("-- ESETROOT_PMAP_ID.: 0x%" PRIx32 "\n", *context.esetroot_window);

    // choose EGL configuration
    EGLConfig config;
    {
        // todo: framebuffer and to check the depth and color and stencil stuff
        // with the screen->root_depth thingy

        // clang-format off
        EGLint attr[] = {
            EGL_SURFACE_TYPE,      EGL_WINDOW_BIT,
            EGL_CONFORMANT,        EGL_OPENGL_BIT,
            EGL_RENDERABLE_TYPE,   EGL_OPENGL_BIT,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

            // todo: get this automatically (from the root visual)
            EGL_RED_SIZE,      8,
            EGL_GREEN_SIZE,    8,
            EGL_BLUE_SIZE,     8,
            EGL_DEPTH_SIZE,   24,

            EGL_ALPHA_SIZE,    8,
            EGL_STENCIL_SIZE,  8,

            // uncomment for multisampled framebuffer
            //EGL_SAMPLE_BUFFERS, 1,
            //EGL_SAMPLES,        4, // 4x MSAA

            EGL_CONFIG_CAVEAT, EGL_NONE,
            EGL_NONE,
        };
        // clang-format on

        EGLint count;
        if (eglChooseConfig(context.display, attr, &config, 1, &count) !=
                EGL_TRUE ||
            count != 1) {
            program_error("Cannot choose EGL config");
        }
    }

    // create EGL surface
    {
        // clang-format off
        EGLint attr[] = {
            // options: EGL_GL_COLORSPACE_LINEAR | EGL_GL_COLORSPACE_SRGB (there are probably more but idc)
            EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB, // EGL_GL_COLORSPACE_SRGB for sRGB framebuffer

            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE,
        };
        // clang-format on

        context.surface = eglCreateWindowSurface(context.display, config,
                                                 *context.xroot_window, attr);
        if (context.surface == EGL_NO_SURFACE) {
            program_error("Cannot create EGL surface");
        }
    }

    // create EGL context
    {
        // clang-format off
        EGLint attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3, // opengl es version 3.3 core profile
            EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#ifndef NDEBUG
            // ask for debug context for non "Release" builds
            // this is so we can enable debug callback
            EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#endif
            EGL_NONE,
        };
        // clang-format on

        context.context =
            eglCreateContext(context.display, config, EGL_NO_CONTEXT, attr);
        if (context.context == EGL_NO_CONTEXT) {
            program_error(
                "Cannot create EGL context, OpenGL 3.3 not supported?");
        }
    }

    ok = eglMakeCurrent(context.display, context.surface, context.surface,
                        context.context);
    Assert(ok && "Failed to make context current");

#ifndef NDEBUG
    // enable debug callback
    glDebugMessageCallback(&DebugCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

    // set vsync
    ok = eglSwapInterval(context.display, (int)opts->vsync);
    Assert(ok && "Failed to set VSync for EGL");

    // LOG("-- Changing root attributes\n");
    // uint32_t event_mask[] = {XCB_EVENT_MASK_EXPOSURE};
    // xcb_change_window_attributes(context.connection, *context.xroot_window,
    //                              XCB_CW_EVENT_MASK, event_mask);

    test_egl_pointers_different_file();

    // get size
    xcb_get_geometry_cookie_t geom_cookie;
    xcb_get_geometry_reply_t *geometry;

    geom_cookie = xcb_get_geometry(context.connection, context.screen->root);
    geometry = xcb_get_geometry_reply(context.connection, geom_cookie, NULL);
    const int width = geometry->width;
    const int height = geometry->height;

    // load the video
    context.video = video_from_file(opts->video_path);

    // sync everything up cuz why not
    xcb_aux_sync(context.connection);
}

static void mainloop() {
    LOG("-- Runninng main loop...\n");

    // Set up the signal handler (so the program would gracefully exit on
    // Ctrl+c)
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;          // No special flags
    sigemptyset(&sa.sa_mask); // No additional signals blocked during handler
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
        const int width = geometry->width;
        const int height = geometry->height;

        // delta time
        struct timespec c2;
        clock_gettime(CLOCK_MONOTONIC, &c2);
        float delta =
            (float)(c2.tv_sec - c1.tv_sec) + 1e-9f * (c2.tv_nsec - c1.tv_nsec);
        c1 = c2;

        // render only if window size is non-zero (minimized)
        if (width != 0 && height != 0) {
            // setup output size covering all client area of window
            glViewport(0, 0, width, height); // for multiple screens maybe i can
                                             // have multiple smaller viewports?

            // clear screen
            glClearColor(0.392f, 0.584f, 0.929f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                    GL_STENCIL_BUFFER_BIT);

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

                shear_thingy = sin((fmodf(da_time, (float)(M_PI) * 2) - (float)(M_PI) * 2));

                float shear[] =  {
                    shear_thingy + 1.0f, 0.0f, 0.0f,
                    0.0f               , 1.0f, 0.0f,
                    0.0f               , 0.0f, 1.0f,
                };

                // const float matrix[9] = { 
                //     1, 0, 0,
                //     0, 1, 0,
                //     0, 0, 1
                // };
                // clang-format on

                glUseProgram(context.video.shader_program);
                glUniformMatrix3fv(glGetUniformLocation(
                                       context.video.shader_program, "rot_mat"),
                                   1, GL_FALSE, matrix);

                glUniformMatrix3fv(
                    glGetUniformLocation(context.video.shader_program,
                                         "shear_mat"),
                    1, GL_FALSE, shear);
            }

            // bind texture to texture unit
            // GLint s_texture =
            //     0; // texture unit that sampler2D will use in GLSL code
            // glBindTextureUnit(s_texture, context.video.texture_id);

            video_render(&context.video, delta);

            // swap the buffers to show outputC-c
            if (!eglSwapBuffers(context.display, context.surface)) {
                program_error("Failed to swap OpenGL buffers!");
            }
        } else {
            // window is minimized, instead sleep a bit
            usleep(10 * 1000);
        }
        if (!keep_running)
            break;
    }
}

static void cleanup() {
    LOG("-- Cleaning up...\n");

    video_clean(&context.video);

    // Destroy the EGL context, surface, and display
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

    // Disconnect from the XCB server
    if (context.connection) {
        xcb_disconnect(context.connection);
    }

    LOG("-- Cleanup complete.\n");
}

static struct argument_options parse_args(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr,
                "Usage: %s <video> <screen_number> "
                "[forced_framerate=0|n] [vsync=true|false]\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *video_file = argv[1];
    int screen_nbr = atoi(argv[2]);

    if (screen_nbr <= 0) {
        program_error("Invalid screen number!\n");
        exit(1);
    }

    bool vsync = true;
    int forced_framerate = 0;

    for (int i = 3; i < argc; i++) {
        char *arg = argv[i];

        char *key = strtok(arg, "=");
        char *value = strtok(arg, "=");

        if (key == NULL || value == NULL)
            continue;

        if (strcmp(key, "--forced_framerate") == 0) {
            forced_framerate = atoi(value);
        } else if (strcmp(key, "--vsync") == 0) {
            vsync = (strcmp(value, "false") == true) ? true : false;
        }
    }

    struct argument_options opts = {.video_path = video_file,
                                    .screen_nbr = screen_nbr,
                                    .vsync = vsync,
                                    .forced_framerate = forced_framerate};

    return opts;
}

int main(int argc, char *argv[]) {
    struct argument_options opts = parse_args(argc, argv);

    setup(&opts);
    mainloop();
    cleanup();

    return EXIT_SUCCESS;
}

static void program_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "-- ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static void handle_sigint(int sig) {
    keep_running = false;
    printf("\nCaught signal %d, gracefully shutting down...\n", sig);
}
