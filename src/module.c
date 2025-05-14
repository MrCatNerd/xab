#include "context.h"
#include "pch.h"
#include "logger.h"
#include "module.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_PLUGINATOR
#include "module_contract_version.h"
#include "module_contract_types.h"

#include "pluginator.h"

static size_t calculate_typelist_size(module_typelist_t typelist);
static size_t calculate_module_type_size(module_type_e type);
static const char *get_module_type_name(module_type_e type);

void load_module(Module_t *module, const char *path, context_t *context) {
    if (!module || !path)
        return;

    module->path = path;

    xab_log(LOG_INFO, "Loading a module from %s\n", path);
    module->plugin = plugin_load(module->path, DL_DEFAULT_FLAGS);
    if (!module->plugin.loaded) {
        goto plugin_invalid_exit;
        return;
    }

    // TODO: more trace logs

    // get the name and the version

    // module_get_name
    FUNCTION_PTR(const char *, module_get_name,
                 plugin_dispatch_function(&module->plugin, "module_get_name"),
                 void);
    if (!module_get_name)
        goto plugin_invalid_exit;
    module->name = (*module_get_name)();

    // module_get_version_major
    FUNCTION_PTR(
        int, module_get_version_major,
        plugin_dispatch_function(&module->plugin, "module_get_version_major"),
        void);
    if (!module_get_version_major)
        goto plugin_invalid_exit;
    module->version_major = (*module_get_version_major)();

    // module_get_version_minor
    FUNCTION_PTR(
        int, module_get_version_minor,
        plugin_dispatch_function(&module->plugin, "module_get_version_minor"),
        void);
    if (!module_get_version_minor)
        goto plugin_invalid_exit;
    module->version_minor = (*module_get_version_minor)();

    // module_get_version_patch
    FUNCTION_PTR(
        int, module_get_version_patch,
        plugin_dispatch_function(&module->plugin, "module_get_version_patch"),
        void);
    if (!module_get_version_patch)
        goto plugin_invalid_exit;
    module->version_patch = (*module_get_version_patch)();

    // get the dispatch function

    // module_get_dispatch
    FUNCTION_PTR(
        void *, module_get_dispatch,
        plugin_dispatch_function(&module->plugin, "module_get_dispatch"), void);
    if (!module_get_dispatch)
        goto plugin_invalid_exit;
    module->dispatch =
        (module_event_error_t (*)(void *data, size_t data_size, void *target,
                                  size_t target_size))(*module_get_dispatch)();

    // verify the contract version matches xab's contract version

    // module_get_contract_version
    FUNCTION_PTR(unsigned int, module_get_contract_version,
                 plugin_dispatch_function(&module->plugin,
                                          "module_get_contract_version"),
                 void);
    if (!module_get_contract_version)
        goto plugin_invalid_exit;

    const unsigned int module_contract_version =
        (*module_get_contract_version)();
    if (module_contract_version != MODULE_CONTRACT_VERSION) {
        xab_log(LOG_WARN,
                "Module '%s': contract version (v%d) is different than xab's "
                "contract version (v%d), unloading module...\n",
                module_contract_version, MODULE_CONTRACT_VERSION);
        goto plugin_exit_general;
    }

    xab_log(LOG_TRACE, "Module '%s': Initalizing...\n", module->name);
    FUNCTION_PTR(module_error_e, module_init,
                 plugin_dispatch_function(&module->plugin, "module_init"),
                 void);
    if (!module_init)
        goto plugin_invalid_exit;
    const module_error_e mod_err = (*module_init)();
    if (mod_err != MODULE_ERROR_NONE)
        goto plugin_invalid_exit;

    // log some data
    xab_log(LOG_INFO, "Loaded module: '%s' v%d.%d.%d (contract v%d)\n",
            module->name, module->version_major, module->version_minor,
            module->version_patch, module_contract_version);

    // calculate and allocate data layouts
    xab_log(LOG_TRACE,
            "Module: '%s': calculating and allocating data layouts\n",
            module->name);
    // reset all sizes and pointers
    module->target_size = 0;
    module->target = NULL;
    module->data_size = 0;
    module->data = NULL;

    // input (data)
    FUNCTION_PTR(module_typelist_t, module_resource_requirements,
                 plugin_dispatch_function(&module->plugin,
                                          "module_resource_requirements"),
                 void);
    if (!module_resource_requirements)
        goto plugin_invalid_exit;
    const module_typelist_t input_types = (*module_resource_requirements)();
    if (input_types.types != NULL && input_types.len > 0) {
        module->data_size = calculate_typelist_size(input_types);

        if (module->data_size > 0)
            module->data = calloc(1, module->data_size);

        size_t offset = 0;
        // if i have requestable data types which are not pointers i might have
        // to use a union
        void *data = NULL;
        for (size_t i = 0; i < input_types.len; i++) {
            module_type_e type = input_types.types[i];
            const size_t type_size = calculate_module_type_size(type);
            switch (type) {
            default:
                xab_log(LOG_WARN,
                        "Module system: module '%s' requested an unknown "
                        "resource, ignoring...\n",
                        module->name);
                break;
            case MODULE_TYPE_VOID:
                xab_log(LOG_WARN,
                        "Module system: module '%s' requested a void resource, "
                        "ignoring...\n",
                        module->name);
                break;
            case MODULE_TYPE_INT:
            case MODULE_TYPE_UINT:
            case MODULE_TYPE_FLOAT:
            case MODULE_TYPE_BOOL:
            case MODULE_TYPE_CHAR:
            case MODULE_TYPE_UCHAR:
            case MODULE_TYPE_CONST_CHAR_PTR:
            case MODULE_TYPE_VOID_PTR:
                xab_log(LOG_WARN,
                        "module type: %s isn't requestable! ignoring...\n",
                        get_module_type_name(type));
                break;
            case MODULE_TYPE_XAB_CONTEXT:
                data = context;
                break;
            case MODULE_TYPE_XCB_CONTEXT_CONNECTION:
                data = context->connection;
                break;
            case MODULE_TYPE_XCB_CONTEXT_ROOT_WINDOW:
                data = &context->screen->root;
                break;
            case MODULE_TYPE_XCB_CONTEXT_DESKTOP_WINDOW:
                data = context->desktop_window;
                break;
            case MODULE_TYPE_EGL_CONTEXT_SURFACE:
                data = context->surface;
                break;
            case MODULE_TYPE_EGL_CONTEXT_DISPLAY:
                data = context->display;
                break;
            case MODULE_TYPE_EGL_CONTEXT:
                data = context->context;
                break;
            }

            memcpy((char *)module->data + offset, &data, type_size);

            offset += type_size;
        }
        if (offset != module->data_size)
            xab_log(LOG_WARN,
                    "resource managment offset isn't equal to the "
                    "data container size (%d != %d)\n",
                    offset, module->data_size);

        free(input_types.types);
    }

    // output (target)
    FUNCTION_PTR(module_typelist_t, module_get_types,
                 plugin_dispatch_function(&module->plugin, "module_get_types"),
                 void);
    if (!module_get_types)
        goto plugin_invalid_exit;
    const module_typelist_t output_types = (*module_get_types)();
    if (output_types.types != NULL && output_types.len > 0) {
        module->target_size = calculate_typelist_size(output_types);
        free(output_types.types);
    }
    if (module->target_size > 0)
        module->target = calloc(1, module->target_size);

    return;
plugin_invalid_exit:
    xab_log(LOG_ERROR, "Module: '%s' is invalid, unloading module...\n",
            module->name ? module->name : path);
plugin_exit_general:
    plugin_unload(&module->plugin);
}

void unload_module(Module_t *module) {
    if (!module)
        return;
    if (module->plugin.loaded) {
        xab_log(LOG_TRACE, "Module '%s': Shutting down...\n", module->name);
        FUNCTION_PTR(
            module_error_e, module_shutdown,
            plugin_dispatch_function(&module->plugin, "module_shutdown"), void);
        if (!module_shutdown)
            xab_log(LOG_WARN,
                    "Module '%s': Dispatching the shutdown function failed!",
                    module->name);
        const module_error_e mod_err = (*module_shutdown)();
        if (mod_err != MODULE_ERROR_NONE)
            xab_log(LOG_WARN, "Module '%s': Shutting down failed!\n");
    }

    xab_log(LOG_INFO, "Unloading module: '%s'\n",
            module->name ? module->name : module->path);

    plugin_unload(&module->plugin);
    module->dispatch = NULL;
}

void module_dispatch(Module_t *module) {
    if (!module)
        return;
    if (!module->plugin.loaded)
        return;

    module_event_error_t event_error = {.event = MODULE_EVENT_NONE,
                                        .error = MODULE_ERROR_NONE};
    if (module->dispatch)
        event_error = (*module->dispatch)(module->data, module->data_size,
                                          module->target, module->target_size);

    if (event_error.error != MODULE_ERROR_NONE)
        xab_log(LOG_ERROR,
                "Module '%s': An error occured while dispatching: %d\n",
                module->name, event_error.error);

    if (event_error.event != MODULE_EVENT_NONE)
        xab_log(LOG_WARN,
                "Module '%s' has dispatched an event but xab doesn't support "
                "events currently, sorry :(\n",
                module->name);
}

static size_t calculate_typelist_size(module_typelist_t typelist) {
    size_t size = 0;
    for (size_t i = 0; i < typelist.len; i++) {
        module_type_e type = typelist.types[i];
        size += calculate_module_type_size(type);
    }

    return size;
}

static size_t calculate_module_type_size(module_type_e type) {
    size_t size = 0;
    switch (type) {
    default:
    case MODULE_TYPE_VOID:
        size = 0;
        break;
    case MODULE_TYPE_INT:
        size = sizeof(int);
        break;
    case MODULE_TYPE_UINT:
        size = sizeof(unsigned int);
        break;
    case MODULE_TYPE_FLOAT:
        size = sizeof(float);
        break;
    case MODULE_TYPE_BOOL:
        size = sizeof(bool);
        break;
    case MODULE_TYPE_CHAR:
        size = sizeof(char);
        break;
    case MODULE_TYPE_UCHAR:
        size = sizeof(unsigned char);
        break;
    case MODULE_TYPE_CONST_CHAR_PTR:
        size = sizeof(const char *);
        break;
    case MODULE_TYPE_VOID_PTR:
        size = sizeof(void *);
        break;
    case MODULE_TYPE_XAB_CONTEXT:
        size = sizeof(context_t *);
        break;
    case MODULE_TYPE_XCB_CONTEXT_CONNECTION:
        size = sizeof(xcb_connection_t *);
        break;
    case MODULE_TYPE_XCB_CONTEXT_ROOT_WINDOW:
        size = sizeof(xcb_window_t *);
        break;
    case MODULE_TYPE_XCB_CONTEXT_DESKTOP_WINDOW:
        size = sizeof(xcb_window_t *);
        break;
    case MODULE_TYPE_EGL_CONTEXT_SURFACE:
        size = sizeof(EGLSurface *);
        break;
    case MODULE_TYPE_EGL_CONTEXT_DISPLAY:
        size = sizeof(EGLDisplay *);
        break;
    case MODULE_TYPE_EGL_CONTEXT:
        size = sizeof(EGLContext *);
        break;
    };

    return size;
}

static const char *get_module_type_name(module_type_e type) {
    const char *name = "unknown";
    switch (type) {
    default:
        break;
    case MODULE_TYPE_VOID:
        name = "void";
        break;
    case MODULE_TYPE_INT:
        name = "int";
        break;
    case MODULE_TYPE_UINT:
        name = "uint";
        break;
    case MODULE_TYPE_FLOAT:
        name = "float";
        break;
    case MODULE_TYPE_BOOL:
        name = "bool";
        break;
    case MODULE_TYPE_CHAR:
        name = "char";
        break;
    case MODULE_TYPE_UCHAR:
        name = "uchar";
        break;
    case MODULE_TYPE_CONST_CHAR_PTR:
        name = "const_char_ptr";
        break;
    case MODULE_TYPE_VOID_PTR:
        name = "void_ptr";
        break;
    case MODULE_TYPE_XAB_CONTEXT:
        name = "xab_context";
        break;
    case MODULE_TYPE_XCB_CONTEXT_CONNECTION:
        name = "xcb_context_connection";
        break;
    case MODULE_TYPE_XCB_CONTEXT_ROOT_WINDOW:
        name = "xcb_context_root_window";
        break;
    case MODULE_TYPE_XCB_CONTEXT_DESKTOP_WINDOW:
        name = "xcb_context_desktop_window";
        break;
    case MODULE_TYPE_EGL_CONTEXT_SURFACE:
        name = "egl_context_surface";
        break;
    case MODULE_TYPE_EGL_CONTEXT_DISPLAY:
        name = "egl_context_display";
        break;
    case MODULE_TYPE_EGL_CONTEXT:
        name = "egl_context";
        break;
    }

    return name;
}

#else
void load_module(Module_t *target, const char *path) {
    (void)target;
    (void)path;
    xab_log(LOG_WARN,
            "xab was compiled with out module system support... ignoring\n");
}
void module_dispatch(Module_t *module) { (void)module; }
void unload_module(Module_t *module) {
    (void)module;
    xab_log(LOG_WARN,
            "xab was compiled with out module system support... ignoring\n");
}
#endif
