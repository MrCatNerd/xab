#include "vdpau_vtable.h"

#include <vdpau/vdpau.h>

#include "logger.h"
#include "utils.h"

int vdpau_vtable_init(VdpauVTable_t *target, VdpDevice device,
                      VdpGetProcAddress *get_proc_address) {
    if (!target || !device || !get_proc_address)
        return -1;

    xab_log(LOG_VERBOSE, "Initializing VDPAU vtable\n");
    target->is_initialized = false;

    struct vdpau_function {
            const int id;
            const int offset;
    };
    const struct vdpau_function vdpau_funcs[] = {
#define VDPAU_FUNCTION(_, id, name) {id, offsetof(VdpauVTable_t, name)},
#include "vdpau_functions.macro_abuse"
#undef VDPAU_FUNCTION
        {0, -1},
    };

    for (const struct vdpau_function *vdpau_func = vdpau_funcs;
         vdpau_func->offset >= 0; vdpau_func++) {
        const void *addr =
            ((char *)target + // convert (char *) cuz of c pointer arithmetic
                              // scaling type shi
             vdpau_func->offset);
        Assert(addr != NULL && "Invalid VPDAU vtable address!");

        VdpStatus status =
            get_proc_address(device, vdpau_func->id, (void **)addr);
        if (status != VDP_STATUS_OK) {
            xab_log(LOG_ERROR,
                    "Error when calling vdp_get_proc_address(function "
                    "id %d): %s\n",
                    vdpau_func->id,
                    target->get_error_string ? target->get_error_string(status)
                                             : "?");
            return -1;
        }
        if ((*(void **)addr) == NULL) {
            xab_log(LOG_ERROR, "Invalid vtable address! (%d)\n",
                    vdpau_func->id);
            return -1;
        }
    }

    target->is_initialized = true;
    return 0;
}
