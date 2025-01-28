#include "pch.h"

#include "setbg.h"

#include "context.h"
#include "atom.h"
#include "logger.h"

// i still need to test these on other wms than awesomewm

static xcb_window_t *find_desktop(context_t *context);
static xcb_window_t *find_desktop_recursive(context_t *context,
                                            xcb_window_t *window,
                                            xcb_atom_t *prop_desktop);
static void set_background_pixmap(context_t *context);

void update_background(context_t *context) {
    TracyCZoneNC(tracy_ctx, "update_background", TRACY_COLOR_BLUE, true);

    xcb_change_property(context->connection, XCB_PROP_MODE_REPLACE,
                        *context->desktop_window, ESETROOT_PMAP_ID,
                        XCB_ATOM_PIXMAP, 32, 1, &context->background_pixmap);
    xcb_change_window_attributes(context->connection, *context->desktop_window,
                                 XCB_CW_BACK_PIXMAP,
                                 &context->background_pixmap);
    xcb_clear_area(context->connection, 0, context->screen->root, 0, 0,
                   context->screen->width_in_pixels,
                   context->screen->height_in_pixels);
    xcb_flush(context->connection);

    TracyCZoneEnd(tracy_ctx);
}

void setup_background(context_t *context) {
    xab_log(LOG_DEBUG, "Creating background pixmap\n");
    // create pixmap
    context->background_pixmap = xcb_generate_id(context->connection);

    xcb_create_pixmap(context->connection, context->screen->root_depth,
                      context->background_pixmap, context->screen->root,
                      context->screen->width_in_pixels,
                      context->screen->height_in_pixels);
    xcb_clear_area(context->connection, 0, context->background_pixmap, 0, 0,
                   context->screen->width_in_pixels,
                   context->screen->height_in_pixels);

    // find the desktop pixmap
    context->desktop_window = find_desktop(context);
    if (context->desktop_window == NULL)
        context->desktop_window = &context->screen->root;

    xab_log(LOG_VERBOSE, "Setting background atoms\n");
    // free the old background if exists
    const char *atom_filters[] = {"_XROOTPMAP", "ESETROOT_PMAP_ID"};
    load_atoms_config_t config = {.only_if_exists = true,
                                  .filters = atom_filters,
                                  .filter_len = 2,
                                  .override = true};
    load_atoms(context, &config);

    /* if ((ESETROOT_PMAP_ID != XCB_ATOM_NONE) &&
        (_XROOTPMAP_ID != XCB_ATOM_NONE)) {
        xcb_get_property_reply_t *reply_xroot = xcb_get_property_reply(
            context->connection,
            xcb_get_property(context->connection, 0, context->screen->root,
                             _XROOTPMAP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0, ~0),
            NULL);

        void *data_xroot = xcb_get_property_value(reply_xroot);

        // kill background
        if (reply_xroot->type == XCB_ATOM) {
            xcb_get_property_reply_t *reply_esetroot = xcb_get_property_reply(
                context->connection,
                xcb_get_property(context->connection, 0, context->screen->root,
                                 ESETROOT_PMAP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0,
                                 ~0),
                NULL);

            void *data_esetroot = xcb_get_property_value(reply_esetroot);

            if (data_xroot && data_esetroot)
                if (reply_esetroot->type == XCB_ATOM_PIXMAP &&
                    *(xcb_pixmap_t *)data_esetroot ==
                        *(xcb_pixmap_t *)data_xroot)
                    xcb_kill_client(context->connection,
                                    *(xcb_pixmap_t *)data_xroot);
            if (reply_esetroot)
                free(reply_esetroot);
        }

        if (reply_xroot)
            free(reply_xroot);
    } */

    config.only_if_exists = false;
    load_atoms(context, &config);

    if (_XROOTPMAP_ID == XCB_ATOM_NONE || ESETROOT_PMAP_ID == XCB_ATOM_NONE) {
        xab_log(LOG_ERROR, "Creation of background pixmap property failed!\n");
        exit(EXIT_FAILURE);
    }

    // set background pixmap
    set_background_pixmap(context);
}

static void set_background_pixmap(context_t *context) {
    const uint32_t *ROOT_WINDOW_EVENT_MASK = (const uint32_t[]){
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE};

    xcb_change_window_attributes(context->connection, context->screen->root,
                                 XCB_CW_EVENT_MASK, (uint32_t[]){0});

    // fetch the background property before we change it, so we can close it
    // later
    xcb_get_property_cookie_t property_cookie = xcb_get_property_unchecked(
        context->connection, false, context->screen->root, ESETROOT_PMAP_ID,
        XCB_ATOM_PIXMAP, 0, 1);

    // set the background's pixmap
    xcb_change_window_attributes(
        context->connection, context->screen->root, XCB_CW_BACK_PIXMAP,
        (const uint32_t[]){context->background_pixmap});
    xcb_clear_area(context->connection, 0, context->screen->root, 0, 0, 0, 0);

    // make pseudo-transparency work by setting the atoms
    xcb_change_property(context->connection, XCB_PROP_MODE_REPLACE,
                        context->screen->root, _XROOTPMAP_ID, XCB_ATOM_PIXMAP,
                        32, 1, &context->background_pixmap);
    xcb_change_property(context->connection, XCB_PROP_MODE_REPLACE,
                        context->screen->root, ESETROOT_PMAP_ID,
                        XCB_ATOM_PIXMAP, 32, 1, &context->background_pixmap);

    // make sure the old wallpaper is freed (only do this for ESETROOT_PMAP_ID)
    xcb_get_property_reply_t *property_reply =
        xcb_get_property_reply(context->connection, property_cookie, NULL);
    if (property_reply && property_reply->value_len) {
        xcb_pixmap_t *root_pixmap = xcb_get_property_value(property_reply);
        if (root_pixmap)
            xcb_kill_client(context->connection, *root_pixmap);
    }
    free((void *)property_reply);

    // make sure our pixmap is not destroyed when we disconnect
    xcb_set_close_down_mode(context->connection,
                            XCB_CLOSE_DOWN_RETAIN_PERMANENT);
    xcb_flush(context->connection);

    xcb_change_window_attributes(context->connection, context->screen->root,
                                 XCB_CW_EVENT_MASK, ROOT_WINDOW_EVENT_MASK);
}

static xcb_window_t *find_desktop(context_t *context) {
    xab_log(LOG_VERBOSE, "Finding desktop background window\n");
    const char *atom_filters[] = {"_NET_WM_WINDOW_TYPE",
                                  "_NET_WM_WINDOW_TYPE_DESKTOP"};

    load_atoms_config_t config = {.only_if_exists = true,
                                  .filters = atom_filters,
                                  .filter_len = 2,
                                  .override = true};
    load_atoms(context, &config);

    if (!_NET_WM_WINDOW_TYPE) {
        xab_log(LOG_DEBUG,
                "Could not find window with _NET_WM_WINDOW_TYPE type "
                "set, defaulting to root window\n");
        return NULL;
    }

    return find_desktop_recursive(context, &context->screen->root,
                                  &_NET_WM_WINDOW_TYPE_DESKTOP);
}

// This is mainly for those weird DEs that don't draw backgrounds to root
// window
static xcb_window_t *find_desktop_recursive(context_t *context,
                                            xcb_window_t *window,
                                            xcb_atom_t *prop_desktop) {
    // check current window
    {
        xcb_get_property_reply_t *reply = xcb_get_property_reply(
            context->connection,
            xcb_get_property(context->connection, false, *window,
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
        context->connection, xcb_query_tree(context->connection, *window),
        NULL);
    if (!reply) {
        xab_log(LOG_FATAL, "xcb_query_tree failed!\n");
        exit(EXIT_FAILURE);
    }

    xcb_window_t *children = xcb_query_tree_children(reply);
    unsigned int n_children =
        (unsigned int)xcb_query_tree_children_length(reply);

    for (unsigned int i = 0; i < n_children; i++) {
        xcb_window_t *child =
            find_desktop_recursive(context, &children[i], prop_desktop);
        if (child == NULL)
            continue;

        return child;
    }
    if (n_children)
        free(reply);

    return NULL;
}
