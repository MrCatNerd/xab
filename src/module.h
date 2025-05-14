#pragma once

#ifdef HAVE_PLUGINATOR
#include "pluginator.h"
#include "module_contract_types.h"

#define MODULE_INITIALIZER                                                     \
    {                                                                          \
        .plugin = {.path = NULL,                                               \
                   .dl_flags = 0,                                              \
                   .dl_handle = NULL,                                          \
                   .link_map = NULL,                                           \
                   .loaded = false,                                            \
                   .symbol_cache = NULL},                                      \
        .name = NULL,                                                          \
        .version_major = -1,                                                   \
        .version_minor = -1,                                                   \
        .version_patch = -1,                                                   \
        .dispatch = NULL,                                                      \
    }

typedef struct Module {
        Plugin_t plugin;

        const char *path;
        const char *name;
        int version_major, version_minor, version_patch;

        // data containers:
        void *data;
        size_t data_size;
        void *target;
        size_t target_size;

        module_event_error_t (*dispatch)(void *data, size_t data_size,
                                         void *target, size_t target_size);
} Module_t;

void load_module(Module_t *module, const char *path, context_t *context);
void module_dispatch(Module_t *module);
void unload_module(Module_t *module);
#endif
