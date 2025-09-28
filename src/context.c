#include "pch.h"

#include <epoxy/common.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <stdlib.h>

#include "context.h"
#include "x_data.h"
#include "egl_stuff.h"
#include "logger.h"
#include "atom.h"
#include "wallpaper.h"
#include "framebuffer.h"
#include "shader_cache.h"
#include "monitor.h"
#include "utils.h"
#include "camera.h"
#include "window.h"

context_t context_create(struct argument_options *opts) {
    context_t context = {0};

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
    int screen_nbr = -1;
    xcb_connection_t *connection = xcb_connect(NULL, &screen_nbr);
    if (connection == NULL || xcb_connection_has_error(connection)) {
        xab_log(LOG_FATAL, "Unable to connect to the X server.\n");
        exit(EXIT_FAILURE);
    }
    Assert(0 >= screen_nbr && "Invalid screen number.");

    xab_log(LOG_DEBUG, "Getting xcb data...\n");
    context.xdata = x_data_from_xcb_connection(connection, screen_nbr);

    xab_log(LOG_VERBOSE,
            "\nInformation of xcb screen %" PRIu32
            ":\n  screen number.: %u\n  width.........: %" PRIu16 "px\n"
            "  height........: %" PRIu16 "px\n"
            "  depth.........: %" PRIu8 " bits\n\n",
            context.xdata.screen->root, context.xdata.screen_nbr,
            context.xdata.screen->width_in_pixels,
            context.xdata.screen->height_in_pixels,
            context.xdata.screen->root_depth);

    xab_log(LOG_DEBUG, "Initializing atom manager\n");
    atom_manager_init();

    xab_log(LOG_DEBUG, "Loading atom list\n");
    load_atom_list(context.xdata.connection, true);

    xab_log(LOG_DEBUG, "Initializing EGL\n");
    // initialize EGL
    context.display =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_XCB_EXT, context.xdata.connection,
                                 (const EGLint[]){
                                     EGL_PLATFORM_XCB_SCREEN_EXT,
                                     context.xdata.screen_nbr,
                                     EGL_NONE,
                                 });

    Assert(context.display != EGL_NO_DISPLAY && "Failed to get EGL display.");

    {
        EGLint major = 0, minor = 0;
        if (!eglInitialize(context.display, &major, &minor)) {
            xab_log(LOG_FATAL, "Cannot initialize EGL display.\n");
            exit(EXIT_FAILURE);
        }
        if (major * 10 + minor < 15) {
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
        xcb_randr_query_version_reply_t *ver_reply =
            xcb_randr_query_version_reply(
                context.xdata.connection,
                xcb_randr_query_version(context.xdata.connection,
                                        XCB_RANDR_MAJOR_VERSION,
                                        XCB_RANDR_MINOR_VERSION),
                NULL);
        if (!ver_reply) {
            // TODO: make the error not fatal, and just not use xcb_randr
            xab_log(LOG_FATAL, "Failed to get RandR version\n");
            xcb_disconnect(context.xdata.connection);
            exit(EXIT_FAILURE);
        }

        xab_log(LOG_DEBUG, "xcb-randr version: %d.%d\n",
                ver_reply->major_version, ver_reply->minor_version);

        free(ver_reply);
#endif /* HAVE_LIBXRANDR */

        // don't assert not on debug mode
        pretty_print_egl_check(
#ifdef ENABLE_OPENGL_DEBUG_CALLBACK
            // true, // TODO: fix weird libmpv stuff
            false,
#else
            false,
#endif
            "eglInitialize call");
    }

    // choose OpenGL API for EGL, by default it uses OpenGL ES
    EGLBoolean ok = eglBindAPI(EGL_OPENGL_API);
    if (ok != EGL_TRUE) {
        xab_log(LOG_FATAL, "Failed to select an OpenGL API for EGL\n");
        // TODO: gracefully shut down
        xcb_disconnect(context.xdata.connection);
        exit(EXIT_FAILURE);
    }

    // initialize window
    context.window = init_window(XBACKGROUND, context.display, &context.xdata);

#ifdef ENABLE_OPENGL_DEBUG_CALLBACK
    // enable debug callback
    glDebugMessageCallback(&DebugCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

    // set vsync
    ok = eglSwapInterval(context.display, (int)opts->vsync);
    Assert(ok && "Failed to set VSync for EGL");

    xab_log(LOG_DEBUG, "Creating camera\n");
    ViewPortConfig_t vpc = {
        .left = 0,
        .right = (float)context.xdata.screen->width_in_pixels,
        .top = 0,
        .bottom = (float)context.xdata.screen->height_in_pixels,
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
        get_monitors(context.xdata.connection, context.xdata.screen);
    context.monitors = monitors_ret.monitors;
    context.monitor_count = monitors_ret.monitor_count;
#endif /* HAVE_LIBXRANDR */

    // if no monitors/randr, use default monitor
    if (context.monitors == NULL || context.monitor_count <= 0) {
        context.monitor_count = 1;
        context.monitors = calloc(1, sizeof(monitor_t));
        create_monitor((monitor_t *)context.monitors, "fullscreen-monitor", 0,
                       true, 0, 0, context.xdata.screen->width_in_pixels,
                       context.xdata.screen->height_in_pixels);
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
    context.scache = create_shader_cache();

    // create main framebuffer
    context.framebuffer = create_framebuffer(
        context.xdata.screen->width_in_pixels,
        context.xdata.screen->height_in_pixels, GL_RGBA, &context.scache);

    // load the videos
    monitor_t fullscreen_monitor;
    create_monitor(&fullscreen_monitor, "fullscreen-monitor", 0, true, 0, 0,
                   context.xdata.screen->width_in_pixels,
                   context.xdata.screen->height_in_pixels);

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

    xab_log(LOG_DEBUG, "Freeing atom manager\n");
    atom_manager_free();

    return context;
}

void context_free(context_t *context) {
    // close and clean up the videos
    for (int i = 0; i < context->wallpaper_count; i++)
        wallpaper_close(&context->wallpapers[i], &context->scache);
    free(context->wallpapers);

    // clean up framebuffer
    delete_framebuffer(&context->framebuffer, &context->scache);

    // clean up shader cache
    shader_cache_cleanup(&context->scache);

    // destroy the EGL display
    xab_log(LOG_DEBUG, "Cleaning EGL display\n");
    if (context->display != EGL_NO_DISPLAY) {
        eglTerminate(context->display);
    }

    // clean up background pixmap
    xab_log(LOG_DEBUG, "Freeing the background pixmap\n");
    destroy_window(&context->window, context->display, &context->xdata);

    // disconnect from the X server
    xab_log(LOG_DEBUG, "Disconnecting from the X server...\n");
    if (context->xdata.connection)
        xcb_disconnect(context->xdata.connection);
}
