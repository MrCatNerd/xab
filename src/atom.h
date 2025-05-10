#pragma once

#include "length_string.h"
#include "pch.h"

#include "hashmap.h"

typedef struct atom_manager {
        struct hashmap *cache;
} atom_manager_t;

typedef struct atom_item {
        const char *name;
        xcb_atom_t atom;
} atom_item_t;

void atom_manager_init(void);

void load_atoms(xcb_connection_t *connection, const char **names, int len,
                bool only_if_exists);

xcb_atom_t get_atom(const char *name);
xcb_atom_t get_atom_or_fallback(const char *name, xcb_atom_t *fallback);

void atom_manager_free(void);

// these atoms will be accessible everywhere, without the need to call get_atom
#define ATOM_COUNT 4
#define ATOMS(X)                                                               \
    X(_XROOTPMAP_ID)                                                           \
    X(ESETROOT_PMAP_ID)                                                        \
    X(_NET_WM_WINDOW_TYPE)                                                     \
    X(_NET_WM_WINDOW_TYPE_DESKTOP)                                             \
    X(_NET_WM_DESKTOP)

// the actuall atoms are created in atoms.c
#define X(atom_name) extern xcb_atom_t atom_name;
ATOMS(X)
#undef X

/// must be called to load the entire atom list
void load_atom_list(xcb_connection_t *connection, bool only_if_exists);
