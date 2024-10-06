#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "atom.h"
#include "context.h"
#include "logging.h"

#define X(atom_name) xcb_atom_t atom_name;
ATOMS(X)
#undef X

static atom_item_t ATOM_LIST[] = {
#define X(atom_name) {#atom_name, strlen(#atom_name), &atom_name},
    ATOMS(X)
#undef X
};

void load_atoms(context_t *context) {
    xcb_intern_atom_cookie_t atoms_cookies[ATOM_COUNT];
    xcb_intern_atom_reply_t *reply = NULL;

    // create / get the atom and get the reply in the XCB way (send all the
    // request at the same time and then get the replies)
    for (int i = 0; i < ATOM_COUNT; i++) {
        atoms_cookies[i] = xcb_intern_atom_unchecked(
            context->connection, false, ATOM_LIST[i].len, ATOM_LIST[i].name);
    }

    for (int i = 0; i < ATOM_COUNT; i++) {
        if (!(reply = xcb_intern_atom_reply(context->connection,
                                            atoms_cookies[i], NULL)))
            continue; // an error occured, get reply for next atom

        *ATOM_LIST[i].atom = reply->atom;
        VLOG("-- atom: %s - 0x%08x\n", ATOM_LIST[i].name, *ATOM_LIST[i].atom);

        free((void *)reply);
    }
}
