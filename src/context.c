#include <stdio.h>
#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>
#include <xcb/randr.h>

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
#include "utils.h"
#include "egl_stuff.h"
#include "atom.h"
#include "context.h"
#include "framebuffer.h"
#include "setbg.h"

context_t context_create(bool vsync) {
    context_t context;

    LOG("-- Checking EGL extensions...\n");
    {
        const char *egl_extensions =
            eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

        Assert(egl_extensions != NULL && "bro fix your computer lmao");

        VLOG("-- EGL extensions: %s\n", egl_extensions);

        char *ext;

        ext = "EGL_EXT_platform_xcb";
        if (!strstr(egl_extensions, ext)) {
            program_error("%s is not supported.\n", ext);
            exit(EXIT_FAILURE);
        } else
            LOG("-- %s supported.\n", ext);
    }

    LOG("-- Connecting to the X server\n");
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
    VLOG("\n");

    // create a pixmap than turn it into the root's pixmap
    LOG("-- Setting up background...\n");
    setup_background(&context);
    /* context.background_pixmap = xcb_generate_id(context.connection);
    xcb_create_pixmap(context.connection, context.screen->root_depth,
                      context.background_pixmap, context.screen->root,
                      context.screen->width_in_pixels,
                      context.screen->height_in_pixels);

    xcb_get_property_cookie_t property_cookie = xcb_get_property_unchecked(
        context.connection, false, context.screen->root, ESETROOT_PMAP_ID,
        XCB_ATOM_PIXMAP, 0, 1);

    xcb_change_window_attributes(context.connection, context.screen->root,
                                 XCB_CW_BACK_PIXMAP,
                                 &context.background_pixmap);
    xcb_clear_area(context.connection, 0, context.screen->root, 0, 0, 0, 0);

    // make pseudo-transparency work by setting the atoms
    xcb_change_property(context.connection, XCB_PROP_MODE_REPLACE,
                        context.screen->root, _XROOTPMAP_ID, XCB_ATOM_PIXMAP,
                        32, 1, &context.background_pixmap);
    xcb_change_property(context.connection, XCB_PROP_MODE_REPLACE,
                        context.screen->root, ESETROOT_PMAP_ID, XCB_ATOM_PIXMAP,
                        32, 1, &context.background_pixmap);
    xcb_flush(context.connection);

    // make sure the old wallpaper is freed (only do this for ESETROOT_PMAP_ID)
    xcb_get_property_reply_t *property_reply =
        xcb_get_property_reply(context.connection, property_cookie, NULL);
    if (property_reply && property_reply->value_len) {
        xcb_pixmap_t *root_pixmap = xcb_get_property_value(property_reply);
        if (root_pixmap) {
        }
        xcb_kill_client(context.connection, *root_pixmap);
    }
    free((void *)property_reply);

    // make sure our pixmap is not destroyed when we disconnect
    xcb_set_close_down_mode(context.connection,
                            XCB_CLOSE_DOWN_RETAIN_PERMANENT); */

    LOG("-- Getting unloaded X server atoms\n");
    load_atoms(&context,
               NULL); // loads the rest of the unloaded atoms (if there are any)
                      // or creates atoms with XCB_ATOM_NONE

    LOG("-- Initializing EGL\n");
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
            program_error("Cannot initialize EGL display.\n");
        }
        if (major < 1 || minor < 5) {
            program_error("EGL version 1.5 or higher required. you are "
                          "currently on version: %d.%d\n",
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

    // choose OpenGL API for EGL, by default it uses OpenGL ES
    EGLBoolean ok = eglBindAPI(EGL_OPENGL_API);
    Assert(ok == EGL_TRUE && "Failed to select OpenGL API for EGL");

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
            program_error("Cannot choose EGL config\n");
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
            program_error("Cannot create EGL surface\n");
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
            program_error(
                "Cannot create EGL context, OpenGL 3.3 not supported?\n");
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
    ok = eglSwapInterval(context.display, (int)vsync);
    Assert(ok && "Failed to set VSync for EGL");

    // create framebuffer
    context.framebuffer = create_framebuffer(context.screen->width_in_pixels,
                                             context.screen->height_in_pixels);

    return context;
}
