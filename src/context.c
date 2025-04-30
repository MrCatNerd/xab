#include "pch.h"

#include <epoxy/common.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

// libepoxy havn't updated their khoronos registry yet, there is an unmerged
// pull request about it and I got nothing to about it until it gets merged :(
#ifndef EGL_EXT_platform_xcb
#define EGL_EXT_platform_xcb 1
#define EGL_PLATFORM_XCB_EXT 0x31DC
#define EGL_PLATFORM_XCB_SCREEN_EXT 0x31DE
#endif /* EGL_EXT_platform_xcb */

#include "context.h"
#include "egl_stuff.h"
#include "logger.h"
#include "atom.h"
#include "wallpaper.h"
#include "framebuffer.h"
#include "shader_cache.h"
#include "setbg.h"
#include "monitor.h"
#include "utils.h"
#include "camera.h"

context_t context_create(struct argument_options *opts) {
    context_t context;

    xab_log(LOG_DEBUG, "Checking EGL extensions...\n");
    {
        const char *egl_extensions =
            eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

        Assert(egl_extensions != NULL && "no EGL extensions found");

        xab_log(LOG_VERBOSE, "EGL extensions: %s\n", egl_extensions);

        char *ext = "EGL_EXT_platform_xcb";
        if (!strstr(egl_extensions, ext)) {
            xab_log(LOG_FATAL, "%s extension is not supported.\n", ext);
            exit(EXIT_FAILURE);
        } else {
            xab_log(LOG_DEBUG, "%s supported.\n", ext);
        }
    }

    xab_log(LOG_DEBUG, "Connecting to the X server\n");
    // open the connection to the X server
    context.connection = xcb_connect(NULL, &context.screen_nbr);

    if (context.connection == NULL ||
        xcb_connection_has_error(context.connection)) {
        xab_log(LOG_FATAL, "Unable to connect to the X server.\n");
        exit(EXIT_FAILURE);
    }

    Assert(0 >= context.screen_nbr && "Invalid screen number.");

    xab_log(LOG_DEBUG, "Getting the first xcb screen\n");
    // get the first screen
    context.screen =
        xcb_setup_roots_iterator(xcb_get_setup(context.connection)).data;

    if (context.screen == NULL) {
        xab_log(LOG_FATAL, "Unable to get the screen.\n");
        // TODO: error handling
    }

    xab_log(LOG_VERBOSE,
            "\nInformation of xcb screen %" PRIu32
            ":\n  screen number.: %u\n  width.........: %" PRIu16 "px\n"
            "  height........: %" PRIu16 "px\n"
            "  depth.........: %" PRIu8 " bits\n\n",
            context.screen->root, context.screen_nbr,
            context.screen->width_in_pixels, context.screen->height_in_pixels,
            context.screen->root_depth);

    // create a pixmap than turn it into the root's pixmap
    xab_log(LOG_DEBUG, "Setting up background...\n");
    setup_background(&context);

    xab_log(LOG_DEBUG, "Getting unloaded X server atoms\n");
    load_atoms(&context,
               NULL); // loads the rest of the unloaded atoms (if there are any)
                      // or creates atoms with XCB_ATOM_NONE

    xab_log(LOG_DEBUG, "Initializing EGL\n");
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
        EGLint major = 0, minor = 0;
        if (!eglInitialize(context.display, &major, &minor)) {
            xab_log(LOG_FATAL, "Cannot initialize EGL display.\n");
            exit(EXIT_FAILURE);
        }
        if (major < 1 || minor < 5) {
            xab_log(LOG_FATAL,
                    "EGL version 1.5 or higher required. you are "
                    "currently on version: %d.%d\n",
                    major, minor);
        } else {
            xab_log(LOG_DEBUG, "EGL version %d.%d\n", major, minor);
        }

        xab_log(LOG_DEBUG, "libepoxy EGL version: %0.1f\n",
                (float)epoxy_egl_version(context.display) / 10);

#ifdef HAVE_LIBXRANDR
        // idk why i chose to do it this way instead of just use the values
        // directly but idc
        xcb_randr_query_version_reply_t *ver_reply =
            xcb_randr_query_version_reply(
                context.connection,
                xcb_randr_query_version(context.connection,
                                        XCB_RANDR_MAJOR_VERSION,
                                        XCB_RANDR_MINOR_VERSION),
                NULL);
        if (!ver_reply) {
            // TODO: make the error not fatal, and just not use xcb_randr
            xab_log(LOG_FATAL, "Failed to get RandR version\n");
            xcb_disconnect(context.connection);
            exit(EXIT_FAILURE);
        }

        xab_log(LOG_DEBUG, "xcb-randr version: %d.%d\n",
                ver_reply->major_version, ver_reply->minor_version);

        free(ver_reply);
#endif /* HAVE_LIBXRANDR */

        // don't assert not on debug mode
#ifndef NDEBUG
        pretty_print_egl_check(false, "eglInitialize call");
#else
        pretty_print_egl_check(true, "eglInitialize call");
#endif
    }

    // choose OpenGL API for EGL, by default it uses OpenGL ES
    EGLBoolean ok = eglBindAPI(EGL_OPENGL_API);
    Assert(ok == EGL_TRUE && "Failed to select OpenGL API for EGL");

    // choose EGL configuration
    EGLConfig config;
    {
        // clang-format off
        EGLint attr[] = {
            EGL_SURFACE_TYPE,      EGL_WINDOW_BIT,
            EGL_CONFORMANT,        EGL_OPENGL_BIT,
            EGL_RENDERABLE_TYPE,   EGL_OPENGL_BIT,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

            EGL_RED_SIZE,      8,
            EGL_GREEN_SIZE,    8,
            EGL_BLUE_SIZE,     8,
            EGL_ALPHA_SIZE,    0,
            EGL_DEPTH_SIZE,   context.screen->root_depth,

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
            xab_log(LOG_FATAL, "Cannot choose EGL config\n");
        }
    }

    // create EGL surface
    {
        // clang-format off
        const EGLint attr[] = {
            // options: EGL_GL_COLORSPACE_LINEAR | EGL_GL_COLORSPACE_SRGB (there are probably more but idc)
            EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB, // EGL_GL_COLORSPACE_SRGB for sRGB framebuffer

            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE,
        };
        // clang-format on

        context.surface = eglCreateWindowSurface(
            context.display, config, context.background_pixmap, attr);
        if (context.surface == EGL_NO_SURFACE)
            xab_log(LOG_FATAL, "Cannot create EGL surface\n");
    }

    // create EGL context
    {
        // clang-format off
        EGLint attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3, // opengl version 3.3 core profile
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
            xab_log(LOG_FATAL,
                    "Cannot create EGL context, OpenGL 3.3 not supported?\n");
        }
    }

    ok = eglMakeCurrent(context.display, context.surface, context.surface,
                        context.context);
    Assert(ok && "Failed to make context current");

#ifdef ENABLE_OPENGL_DEBUG_CALLBACK
    // enable debug callback
    glDebugMessageCallback(&DebugCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

    // set vsync
    ok = eglSwapInterval(context.display, (int)opts->vsync);
    Assert(ok && "Failed to set VSync for EGL");

    xab_log(LOG_DEBUG, "Creating camera\n");
    ViewPortConfig_t vpc = {.left = 0,
                            .right = (float)context.screen->width_in_pixels,
                            .top = 0,
                            .bottom = (float)context.screen->height_in_pixels,
                            .near = -1,
                            .far = 1};

    context.camera = create_camera(0.0f, 0.0f, 0.0f, vpc);

    // set everything to zero
    context.monitors = NULL;
    context.monitor_count = 0;
    // get monitors if randr
#ifdef HAVE_LIBXRANDR
    xab_log(LOG_DEBUG, "Getting monitors\n");
    get_monitors_t monitors_ret =
        get_monitors(context.connection, context.screen);
    context.monitors = monitors_ret.monitors;
    context.monitor_count = monitors_ret.monitor_count;
#endif /* HAVE_LIBXRANDR */

    // if no monitors/randr, use default monitor
    if (context.monitors == NULL || context.monitor_count <= 0) {
        context.monitor_count = 1;
        context.monitors = calloc(1, sizeof(monitor_t));
        create_monitor((monitor_t *)context.monitors, "fullscreen-monitor", 0,
                       true, 0, 0, context.screen->width_in_pixels,
                       context.screen->height_in_pixels);
    }

    // TODO: verbose macro ifdef thingy
    for (int i = 0; i < context.monitor_count; i++)
        xab_log(LOG_VERBOSE, "Monitor [%d] x: %d, y: %d, w: %d, h: %d\n", i,
                (context.monitors[i])->x, (context.monitors[i])->y,
                (context.monitors[i])->width, (context.monitors[i])->height);

    // allocate wallpapers and set wallpaper_count
    context.wallpaper_count = opts->n_wallpaper_options;
    context.wallpapers = calloc(sizeof(wallpaper_t), context.wallpaper_count);

    // allocate the shader cache
    context.scache = create_shader_cache(2); // reserve 2 shaders in the cache

    // create main framebuffer
    context.framebuffer = create_framebuffer(context.screen->width_in_pixels,
                                             context.screen->height_in_pixels,
                                             GL_RGBA, &context.scache);

    // load the videos
    monitor_t fullscreen_monitor;
    create_monitor(&fullscreen_monitor, "fullscreen-monitor", 0, true, 0, 0,
                   context.screen->width_in_pixels,
                   context.screen->height_in_pixels);

    for (int i = 0; i < context.wallpaper_count; i++) {
        int idx = opts->wallpaper_options[i].monitor;

        if (idx > context.monitor_count || idx == 0) {
            xab_log(LOG_WARN,
                    "Invalid monitor index: %d, defaulting to max (%d)\n", idx,
                    context.monitor_count);
            idx = context.monitor_count;
        }
        monitor_t *monitor = NULL;
        if (idx < 0)
            monitor = &fullscreen_monitor;
        else
            monitor = context.monitors[idx - 1];

        wallpaper_init(1.0f, monitor->width, monitor->height,
                       monitor->x + opts->wallpaper_options[i].offset_x,
                       monitor->y + opts->wallpaper_options[i].offset_y,
                       opts->wallpaper_options[i].pixelated,
                       opts->wallpaper_options[i].video_path,
                       &context.wallpapers[i], opts->hw_accel, &context.scache);
    }

    // cleanup the monitors cuz we just copied the monitor data to the wallpaper
    // thingy
    cleanup_monitors(context.monitor_count, context.monitors);

    return context;
}
