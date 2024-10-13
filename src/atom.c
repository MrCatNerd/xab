#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "atom.h"
#include "context.h"
#include "logging.h"

#define X(atom_name) xcb_atom_t atom_name = XCB_ATOM_NONE;
ATOMS(X)
#undef X

static atom_item_t ATOM_LIST[] = {
#define X(atom_name) {#atom_name, strlen(#atom_name), &atom_name},
    ATOMS(X)
#undef X
};

void load_atoms(context_t *context, load_atoms_config_t *config) {
    xcb_intern_atom_cookie_t atoms_cookies[ATOM_COUNT];
    xcb_intern_atom_reply_t *reply = NULL;

    if (!config) {
        const load_atoms_config_t default_config = {.only_if_exists = false,
                                                    .filters = NULL,
                                                    .filter_len = 0,
                                                    .override = false};
        config = (load_atoms_config_t *)&default_config;
    }

    // create / get the atom and get the reply in the XCB way (send all the
    // request at the same time and then get the replies)

    // this is getting really messy
    unsigned int atom_count = 0;
    unsigned int index_map[ATOM_COUNT];
    for (unsigned int i = 0; i < ATOM_COUNT; i++) {
        bool should_continue = false;
        if (config->filter_len)
            for (unsigned int j = 0; j < config->filter_len; j++) {
                LOG("skipping atom: %s\n", ATOM_LIST[i].name);
                if ((!strcmp(ATOM_LIST[i].name, config->filters[j]) ||
                     (!config->override && ATOM_LIST[i].atom != XCB_ATOM_NONE)))
                    should_continue = true;
            }
        if (should_continue)
            continue;

        index_map[atom_count++] = i;

        atoms_cookies[i] = xcb_intern_atom_unchecked(
            context->connection, config->only_if_exists, ATOM_LIST[i].len,
            ATOM_LIST[i].name);
    }

    for (unsigned int i = 0; i < atom_count; i++) {
        const unsigned int index = index_map[i];

        if (!(reply = xcb_intern_atom_reply(context->connection,
                                            atoms_cookies[index], NULL))) {
            // an error occured, get reply for next atom
            *ATOM_LIST[index].atom = XCB_ATOM_NONE;
            continue;
        }

        printf("length: %d\n", reply->response_type);

        *ATOM_LIST[index].atom = reply->atom;
        LOG("-- atom: %s - 0x%08x\n", ATOM_LIST[index].name,
            *ATOM_LIST[index].atom);

        free((void *)reply);
    }
}
