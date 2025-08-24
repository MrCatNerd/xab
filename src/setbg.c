#include "pch.h"

#include "setbg.h"

#include "context.h"
#include "atom.h"
#include "logger.h"
#include "x_data.h"

// i still need to test these on other wms than awesomewm

static xcb_window_t *find_desktop(x_data_t *xdata);
static xcb_window_t *find_desktop_recursive(x_data_t *xdata,
                                            xcb_window_t *window,
                                            xcb_atom_t *prop_desktop);
static void set_background_pixmap(xcb_pixmap_t pixmap, x_data_t *xdata);

void update_background(xcb_pixmap_t *pixmap, x_data_t *xdata,
                       xcb_window_t *desktop_window) {
    TracyCZoneNC(tracy_ctx, "update_background", TRACY_COLOR_BLUE, true);
    Assert(pixmap != NULL && xdata != NULL);

    xcb_change_property(xdata->connection, XCB_PROP_MODE_REPLACE,
                        *desktop_window, ESETROOT_PMAP_ID, XCB_ATOM_PIXMAP, 32,
                        1, pixmap);
    xcb_change_window_attributes(xdata->connection, *desktop_window,
                                 XCB_CW_BACK_PIXMAP, pixmap);
    xcb_clear_area(xdata->connection, 0, xdata->screen->root, 0, 0,
                   xdata->screen->width_in_pixels,
                   xdata->screen->height_in_pixels);
    xcb_flush(xdata->connection);

    TracyCZoneEnd(tracy_ctx);
}

xcb_window_t *setup_background(xcb_pixmap_t window_pixmap, x_data_t *xdata) {
    Assert(window_pixmap != NULL && xdata != NUULL && "Invalid pointers");

    // find the desktop pixmap
    xcb_window_t *desktop_window = find_desktop(xdata);
    if (desktop_window == NULL)
        desktop_window = &xdata->screen->root;

    xab_log(LOG_VERBOSE, "Setting background atoms\n");

    // free the old background if exists
    const char *atoms[] = {"_XROOTPMAP_ID", "ESETROOT_PMAP_ID"};
    load_atoms(xdata->connection, atoms, 2, true);
    _XROOTPMAP_ID = get_atom_or_fallback("_XROOTPMAP_ID", &_XROOTPMAP_ID);
    ESETROOT_PMAP_ID =
        get_atom_or_fallback("ESETROOT_PMAP_ID", &ESETROOT_PMAP_ID);

    if ((ESETROOT_PMAP_ID != XCB_ATOM_NONE) &&
        (_XROOTPMAP_ID != XCB_ATOM_NONE)) {
        xcb_get_property_reply_t *reply_xroot = xcb_get_property_reply(
            xdata->connection,
            xcb_get_property(xdata->connection, 0, xdata->screen->root,
                             _XROOTPMAP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0, ~0),
            NULL);

        void *data_xroot = xcb_get_property_value(reply_xroot);

        // kill background
        if (reply_xroot->type == XCB_ATOM) {
            xcb_get_property_reply_t *reply_esetroot = xcb_get_property_reply(
                xdata->connection,
                xcb_get_property(xdata->connection, 0, xdata->screen->root,
                                 ESETROOT_PMAP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0,
                                 ~0),
                NULL);

            void *data_esetroot = xcb_get_property_value(reply_esetroot);

            if (data_xroot && data_esetroot)
                if (reply_esetroot->type == XCB_ATOM_PIXMAP &&
                    *(xcb_pixmap_t *)data_esetroot ==
                        *(xcb_pixmap_t *)data_xroot)
                    xcb_kill_client(xdata->connection,
                                    *(xcb_pixmap_t *)data_xroot);
            if (reply_esetroot)
                free(reply_esetroot);
        }

        if (reply_xroot)
            free(reply_xroot);
    }

    load_atoms(xdata->connection, atoms, 2, false);
    _XROOTPMAP_ID = get_atom_or_fallback("_XROOTPMAP", &_XROOTPMAP_ID);
    ESETROOT_PMAP_ID =
        get_atom_or_fallback("ESETROOT_PMAP_ID", &ESETROOT_PMAP_ID);

    if (_XROOTPMAP_ID == XCB_ATOM_NONE || ESETROOT_PMAP_ID == XCB_ATOM_NONE) {
        xab_log(LOG_FATAL, "Creation of background pixmap property failed!\n");
        xcb_disconnect(xdata->connection);
        exit(EXIT_FAILURE);
    }

    // set background pixmap
    set_background_pixmap(window_pixmap, xdata);
    return desktop_window;
}

static void set_background_pixmap(xcb_pixmap_t pixmap, x_data_t *xdata) {
    const uint32_t *ROOT_WINDOW_EVENT_MASK = (const uint32_t[]){
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE};

    xcb_change_window_attributes(xdata->connection, xdata->screen->root,
                                 XCB_CW_EVENT_MASK, (uint32_t[]){0});

    // fetch the background property before we change it, so we can close it
    // later
    xcb_get_property_cookie_t property_cookie = xcb_get_property_unchecked(
        xdata->connection, false, xdata->screen->root, ESETROOT_PMAP_ID,
        XCB_ATOM_PIXMAP, 0, 1);

    // set the background's pixmap
    xcb_change_window_attributes(xdata->connection, xdata->screen->root,
                                 XCB_CW_BACK_PIXMAP,
                                 (const uint32_t[]){pixmap});
    xcb_clear_area(xdata->connection, 0, xdata->screen->root, 0, 0, 0, 0);

    // make pseudo-transparency work by setting the atoms
    xcb_change_property(xdata->connection, XCB_PROP_MODE_REPLACE,
                        xdata->screen->root, _XROOTPMAP_ID, XCB_ATOM_PIXMAP, 32,
                        1, &pixmap);
    xcb_change_property(xdata->connection, XCB_PROP_MODE_REPLACE,
                        xdata->screen->root, ESETROOT_PMAP_ID, XCB_ATOM_PIXMAP,
                        32, 1, &pixmap);

    // make sure the old wallpaper is freed (only do this for ESETROOT_PMAP_ID)
    xcb_get_property_reply_t *property_reply =
        xcb_get_property_reply(xdata->connection, property_cookie, NULL);
    if (property_reply && property_reply->value_len) {
        xcb_pixmap_t *root_pixmap = xcb_get_property_value(property_reply);
        if (root_pixmap)
            xcb_kill_client(xdata->connection, *root_pixmap);
    }
    free((void *)property_reply);

    // make sure our pixmap is not destroyed when we disconnect
    xcb_set_close_down_mode(xdata->connection, XCB_CLOSE_DOWN_RETAIN_PERMANENT);
    xcb_flush(xdata->connection);

    xcb_change_window_attributes(xdata->connection, xdata->screen->root,
                                 XCB_CW_EVENT_MASK, ROOT_WINDOW_EVENT_MASK);
}

static xcb_window_t *find_desktop(x_data_t *xdata) {
    xab_log(LOG_VERBOSE, "Finding desktop background window\n");
    const char *atoms[] = {"_NET_WM_WINDOW_TYPE",
                           "_NET_WM_WINDOW_TYPE_DESKTOP"};
    load_atoms(xdata->connection, atoms, 2, true);
    _NET_WM_WINDOW_TYPE =
        get_atom_or_fallback("_NET_WM_WINDOW_TYPE", &_NET_WM_WINDOW_TYPE);
    _NET_WM_WINDOW_TYPE_DESKTOP = get_atom_or_fallback(
        "_NET_WM_WINDOW_TYPE_DESKTOP", &_NET_WM_WINDOW_TYPE_DESKTOP);

    if (!_NET_WM_WINDOW_TYPE) {
        xab_log(LOG_DEBUG,
                "Could not find window with _NET_WM_WINDOW_TYPE type "
                "set, defaulting to root window\n");
        return NULL;
    }

    return find_desktop_recursive(xdata, &xdata->screen->root,
                                  &_NET_WM_WINDOW_TYPE_DESKTOP);
}

// This is mainly for those weird DEs that don't draw backgrounds to root
// window
static xcb_window_t *find_desktop_recursive(x_data_t *xdata,
                                            xcb_window_t *window,
                                            xcb_atom_t *prop_desktop) {
    // check current window
    {
        xcb_get_property_reply_t *reply = xcb_get_property_reply(
            xdata->connection,
            xcb_get_property(xdata->connection, false, *window,
                             _NET_WM_WINDOW_TYPE, XCB_ATOM, 0L,
                             sizeof(xcb_atom_t)),
            NULL);

        if (!reply)
            return NULL;

        if (xcb_get_property_value_length(reply) > 0 &&
            reply->type == XCB_ATOM && reply->format == 32) {
            xcb_atom_t *ret_props = (xcb_atom_t *)xcb_get_property_value(reply);
            unsigned int n_ret =
                xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);

            for (unsigned int i = 0; i < n_ret; i++) {
                xcb_atom_t prop = ret_props[i];
                if (prop == *prop_desktop) {
                    free(reply);
                    return window;
                }
            }
            free(reply);

            return ret_props;
        }
    }

    // otherwise, check children
    xcb_query_tree_reply_t *reply = xcb_query_tree_reply(
        xdata->connection, xcb_query_tree(xdata->connection, *window), NULL);
    if (!reply) {
        xab_log(LOG_FATAL, "xcb_query_tree failed!\n");
        xcb_disconnect(xdata->connection);
        exit(EXIT_FAILURE);
    }

    xcb_window_t *children = xcb_query_tree_children(reply);
    unsigned int n_children =
        (unsigned int)xcb_query_tree_children_length(reply);

    for (unsigned int i = 0; i < n_children; i++) {
        xcb_window_t *child =
            find_desktop_recursive(xdata, &children[i], prop_desktop);
        if (child == NULL)
            continue;

        return child;
    }
    if (n_children)
        free(reply);

    return NULL;
}
