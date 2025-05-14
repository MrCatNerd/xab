#pragma once

#define BIT(n) (1 << (n))

// *starting to regret for not choosing IPC*

#include <stddef.h>

typedef enum module_type {
    MODULE_TYPE_VOID = 0,
    MODULE_TYPE_INT = 1,
    MODULE_TYPE_UINT = 2,
    MODULE_TYPE_FLOAT = 3,
    MODULE_TYPE_BOOL = 4,
    MODULE_TYPE_CHAR = 5,
    MODULE_TYPE_UCHAR = 6,
    MODULE_TYPE_CONST_CHAR_PTR = 7,
    MODULE_TYPE_VOID_PTR = 8,

    // requestable types, i don't wanna seperate them yet cuz im lazy
    MODULE_TYPE_XAB_CONTEXT = 9,
    MODULE_TYPE_XCB_CONTEXT_CONNECTION = 10,
    MODULE_TYPE_XCB_CONTEXT_ROOT_WINDOW = 11,
    MODULE_TYPE_XCB_CONTEXT_DESKTOP_WINDOW = 12,
    MODULE_TYPE_EGL_CONTEXT_SURFACE = 13,
    MODULE_TYPE_EGL_CONTEXT_DISPLAY = 14,
    MODULE_TYPE_EGL_CONTEXT = 15,
} module_type_e;

// TODO: implement events in the module engine
typedef enum module_event {
    MODULE_EVENT_NONE = 0,
    MODULE_EVENT_RELOAD_ALL = BIT(1),
    MODULE_EVENT_RELOAD_DIPSATCH = BIT(2),
    MODULE_EVENT_RELOAD_RESOURCES = BIT(3),
    MODULE_EVENT_RELOAD_TARGET_DATA = BIT(4),
    MODULE_EVENT_RELOAD_UNLOAD = BIT(5),
} module_event_e;

typedef enum module_error {
    MODULE_ERROR_NONE = 0,
    MODULE_ERROR_GENERAL = BIT(1),
    MODULE_ERROR_INVALID_MEMORY = BIT(2),
    MODULE_ERROR_INVALID_ARGUMENTS = BIT(3),
    MODULE_ERROR_INVALID_ARGUMENTS_SIZE = BIT(4),
    MODULE_ERROR_XERROR = BIT(5),
} module_error_e;

typedef struct module_event_error {
        module_event_e event;
        module_error_e error;
} module_event_error_t;

typedef struct module_typelist {
        size_t len;
        module_type_e *types;
} module_typelist_t;
