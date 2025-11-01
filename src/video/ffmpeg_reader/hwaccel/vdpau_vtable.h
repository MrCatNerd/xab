#pragma once

#include <stdbool.h>
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>

typedef struct VdpauVTable {
        bool is_initialized;
#define VDPAU_FUNCTION(type, _, name) type *name;
#include "vdpau_functions.macro_abuse"
#undef VDPAU_FUNCTION
} VdpauVTable_t;

/// if this function fails it invalidate target
int vdpau_vtable_init(VdpauVTable_t *target, VdpDevice device,
                      VdpGetProcAddress *get_proc_address);
