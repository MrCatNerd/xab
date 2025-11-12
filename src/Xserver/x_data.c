#include "Xserver/x_data.h"

#include <stdlib.h>

#include "logger.h"
#include "utils.h"

x_data_t x_data_init(void) {
    x_data_t xdata = {
        .connection = NULL,
        .visual = NULL,
        .screen = NULL,
        .screen_nbr = -1,
    };

    xab_log(LOG_DEBUG, "Connecting to the X server\n");
    xdata.connection = xcb_connect(NULL, &xdata.screen_nbr);
    if (xdata.connection == NULL ||
        xcb_connection_has_error(xdata.connection)) {
        xab_log(LOG_FATAL, "Unable to connect to the X server.\n");
        exit(EXIT_FAILURE);
    }
    Assert(xdata.screen_nbr >= 0 && "Invalid screen number.");

    xab_log(LOG_DEBUG, "Getting the first xcb screen\n");
    xdata.screen =
        xcb_setup_roots_iterator(xcb_get_setup(xdata.connection)).data;
    Assert(xdata.screen != NULL && "Unable to get the xcb screen!");

    xab_log(LOG_DEBUG, "Getting xcb root visual\n");
    xcb_depth_iterator_t depth_iter =
        xcb_screen_allowed_depths_iterator(xdata.screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        xcb_depth_t *depth = depth_iter.data;
        xcb_visualtype_iterator_t vis_iter = xcb_depth_visuals_iterator(depth);

        for (; vis_iter.rem; xcb_visualtype_next(&vis_iter)) {
            if (vis_iter.data->visual_id == xdata.screen->root_visual) {
                xdata.visual = vis_iter.data;
                break;
            }
        }
        if (xdata.visual)
            break;
    }
    Assert(xdata.visual != NULL && "Unable to get the xcb root visual!");

    return xdata;
}

void x_data_free(x_data_t *xdata) {
    Assert(xdata != NULL && "Invalid xdata pointer!");

    xab_log(LOG_DEBUG, "Disconnecting from the X server...\n");
    if (xdata->connection)
        xcb_disconnect(xdata->connection);
    else
        xab_log(LOG_WARN, "No X connection specified!\n");
}
