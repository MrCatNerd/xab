#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#include "module_contract_types.h"
#include "module_contract_version.h"

typedef struct MousePosition {
        float mousex;
        float mousey;
} MousePosition_t;

typedef struct module_mouse_inputs {
        xcb_connection_t *connection;
        xcb_window_t *root;
} module_mouse_inputs_t;

unsigned int module_get_contract_version(void) {
    return MODULE_CONTRACT_VERSION;
}
const char *module_get_name(void) { return "mouse"; }
int module_get_version_major(void) { return 1; }
int module_get_version_minor(void) { return 0; }
int module_get_version_patch(void) { return 0; }

const char *module_verify_data(void *data, unsigned long size) {
    if (size < sizeof(module_mouse_inputs_t))
        return "data size is too small";

    module_mouse_inputs_t *converted_data = data;
    if (!converted_data->connection || !converted_data->root)
        return "one or more of the converted data pointers are NULL";

    return NULL;
}

module_typelist_t module_get_types(void) {
    const size_t types_size = 2;
    module_type_e *types = calloc(types_size, sizeof(module_type_e));
    types[0] = MODULE_TYPE_FLOAT;
    types[1] = MODULE_TYPE_FLOAT;

    module_typelist_t typelist = {.len = types_size, .types = types};
    return typelist;
}

module_typelist_t module_resource_requirements(void) {
    const size_t types_size = 2;
    module_type_e *types = calloc(types_size, sizeof(module_type_e));
    types[0] = MODULE_TYPE_XCB_CONTEXT_CONNECTION;
    types[1] = MODULE_TYPE_XCB_CONTEXT_ROOT_WINDOW;

    module_typelist_t typelist = {.len = types_size, .types = types};
    return typelist;
}

module_error_e module_init(void) { return MODULE_ERROR_NONE; }

module_event_error_t module_dispatch(void *data, size_t data_size, void *target,
                                     size_t target_size) {
    module_event_error_t event_error = {.error = MODULE_ERROR_NONE,
                                        .event = MODULE_EVENT_NONE};

    if (!data || !target) {
        event_error.error |= MODULE_ERROR_INVALID_ARGUMENTS;
        return event_error;
    } else if (target_size < sizeof(float) || data_size < 2 * sizeof(void *)) {
        event_error.error |= MODULE_ERROR_INVALID_ARGUMENTS_SIZE;
        return event_error;
    }

    MousePosition_t *target_mousepos = (MousePosition_t *)target;

    const module_mouse_inputs_t *converted_data = data;
    xcb_connection_t *connection = converted_data->connection;
    xcb_window_t *root = converted_data->root;

    xcb_query_pointer_cookie_t pointer_cookie =
        xcb_query_pointer(connection, *root);
    xcb_query_pointer_reply_t *pointer_reply =
        xcb_query_pointer_reply(connection, pointer_cookie, NULL);

    if (pointer_reply) {
        target_mousepos->mousex = pointer_reply->root_x;
        target_mousepos->mousey = pointer_reply->root_y;
        free(pointer_reply);
    } else {
        event_error.error |= MODULE_ERROR_XERROR;
        target_mousepos->mousex = 0;
        target_mousepos->mousey = 0;
    }

    return event_error;
}

void *module_get_dispatch(void) { return (void *)&module_dispatch; }

module_error_e module_shutdown(void) { return MODULE_ERROR_NONE; }
