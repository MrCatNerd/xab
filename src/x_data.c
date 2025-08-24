#include "x_data.h"
#include "logger.h"

x_data_t x_data_from_xcb_connection(xcb_connection_t *connection,
                                    int screen_nbr) {
    Assert(connection != NULL && "Invalid xcb connection!");

    x_data_t xdata = {0};
    xdata.connection = connection;
    xdata.screen_nbr = screen_nbr;

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
