#include "logger.h"
#include "pch.h"

#include <stdint.h>
#include <xcb/xproto.h>

#include "hashmap.h"
#include "atom.h"

atom_manager_t atom_manager = {.cache = NULL};

// create atoms
#define X(atom_name) xcb_atom_t atom_name = XCB_ATOM_NONE;
ATOMS(X)
#undef X

static int atom_manager_compare(const void *a, const void *b, void *udata) {
    const atom_item_t *atom_item_a = (const atom_item_t *)a;
    const atom_item_t *atom_item_b = (const atom_item_t *)b;
    return strcmp(atom_item_a->name, atom_item_b->name);
}

static uint64_t atom_manager_hash(const void *item, uint64_t seed0,
                                  uint64_t seed1) {
    const atom_item_t *atom_item = (const atom_item_t *)item;
    return hashmap_sip(atom_item->name, strlen(atom_item->name), seed0, seed1);
}

void atom_manager_init(void) {
    if (atom_manager.cache)
        return;

    atom_manager.cache =
        hashmap_new(sizeof(atom_item_t), 0, 0, 0, atom_manager_hash,
                    atom_manager_compare, NULL, NULL);
}

void load_atoms(xcb_connection_t *connection, const char **names, int len,
                bool only_if_exists) {
    // send all of the requests at once
    union {
            xcb_intern_atom_cookie_t cookie;
            xcb_intern_atom_reply_t *reply;
    } *data = calloc(len, sizeof(union {
                         xcb_intern_atom_cookie_t cookie;
                         xcb_intern_atom_reply_t *reply;
                     }));

    for (int i = 0; i < len; i++) {
        const char *name = names[i];
        data[i].cookie = xcb_intern_atom_unchecked(connection, only_if_exists,
                                                   strlen(name), name);
    }

    // get all of the responses
    for (int i = 0; i < len; i++) {
        data[i].reply = xcb_intern_atom_reply(connection, data[i].cookie, NULL);
        hashmap_set(
            atom_manager.cache,
            &(atom_item_t){.name = names[i], .atom = data[i].reply->atom});
        free((void *)data[i].reply);
    }

    free((void *)data);
}

xcb_atom_t get_atom(const char *name) {
    atom_item_t *atom = (atom_item_t *)hashmap_get(
        atom_manager.cache, &(atom_item_t){.name = name});
    if (!atom)
        return XCB_ATOM_NONE;
    return atom->atom;
}

xcb_atom_t get_atom_or_fallback(const char *name, xcb_atom_t *fallback) {
    xcb_atom_t atom = get_atom(name);
    if (atom == XCB_ATOM_NONE)
        atom = *fallback;
    return atom;
}

void atom_manager_free(void) {
    if (!atom_manager.cache)
        return;

    hashmap_free(atom_manager.cache);
    atom_manager.cache = NULL;
}

void load_atom_list(xcb_connection_t *connection, bool only_if_exists) {
#define X(atom_name) #atom_name,
    const char *atom_list[] = {ATOMS(X)};
#undef X
    int atom_list_len = sizeof(atom_list) / sizeof(*atom_list);

    load_atoms(connection, atom_list, atom_list_len, only_if_exists);

#define X(atom_name) atom_name = get_atom_or_fallback(#atom_name, &atom_name);
    ATOMS(X);
#undef X
}
