#pragma once

#include "pch.h"

#include "context.h"

// i need to rework the atom system, but i have better things to do currently

// oh boi its time to abuse macros
typedef struct atom_item {
        const char *name;
        size_t len;
        xcb_atom_t *atom;
} atom_item_t;

#define ATOM_COUNT 4
#define ATOMS(X)                                                               \
    X(_XROOTPMAP_ID)                                                           \
    X(ESETROOT_PMAP_ID)                                                        \
    X(_NET_WM_WINDOW_TYPE)                                                     \
    X(_NET_WM_WINDOW_TYPE_DESKTOP)                                             \
    X(_NET_WM_DESKTOP)

#define X(atom_name) extern xcb_atom_t atom_name;
ATOMS(X)
#undef X

/// todo: document this
typedef struct load_atom_config {
        bool only_if_exists;
        const char **filters;
        unsigned int filter_len;
        bool override;
} load_atoms_config_t;

/// loads atoms from the x server, if args is NULL than
/// args is ignored
void load_atoms(context_t *context, load_atoms_config_t *config);

/// returns the value of an atom
void *get_atom_value(context_t *context, xcb_atom_t *atom);
