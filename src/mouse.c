#include <stdlib.h>

#include "mouse.h"
#include "context.h"
#include "logger.h"

MousePosition_t get_mouse_position(context_t *context) {
    xcb_query_pointer_cookie_t pointer_cookie;
    xcb_query_pointer_reply_t *pointer_reply;

    pointer_cookie =
        xcb_query_pointer(context->connection, context->screen->root);
    pointer_reply =
        xcb_query_pointer_reply(context->connection, pointer_cookie, NULL);

    if (pointer_reply) {
        xab_log(LOG_VERBOSE, "Mouse Position: (%d, %d)\n",
                pointer_reply->root_x, pointer_reply->root_y);
        MousePosition_t position = {pointer_reply->root_x,
                                    pointer_reply->root_y};
        free(pointer_reply);
        return position;
    } else {
        xab_log(LOG_ERROR, "Failed to query pointer position.\n");
        MousePosition_t position = {0, 0};
        return position;
    }
}
