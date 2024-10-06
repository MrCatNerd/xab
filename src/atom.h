#pragma once

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

#include "context.h"

// oh boi its time to abuse macros
typedef struct atom_item {
        const char *name;
        size_t len;
        xcb_atom_t *atom;
} atom_item_t;

#define ATOM_COUNT 3
#define ATOMS(X)                                                               \
    X(_XROOTPMAP_ID)                                                           \
    X(ESETROOT_PMAP_ID)                                                        \
    X(_XSETROOT_ID)

#define X(atom_name) extern xcb_atom_t atom_name;
ATOMS(X)
#undef X

void load_atoms(context_t *context);
