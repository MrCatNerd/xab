#include <stddef.h>

#include "module_contract_types.h"
#include "module_contract_version.h"

unsigned int module_get_contract_version(void) {
    return MODULE_CONTRACT_VERSION;
}
const char *module_get_name(void) { return "dummy"; }
int module_get_version_major(void) { return 0; }
int module_get_version_minor(void) { return 0; }
int module_get_version_patch(void) { return 0; }

const char *module_verify_data(void *data, unsigned long size) {
    (void)data;
    (void)size;
    return NULL;
}

module_typelist_t module_get_types(void) {
    const module_typelist_t typelist = {.len = 0, .types = NULL};
    return typelist;
}

module_typelist_t module_resource_requirements(void) {
    const module_typelist_t typelist = {.len = 0, .types = NULL};
    return typelist;
}

module_error_e module_init(void) { return MODULE_ERROR_NONE; }

module_event_error_t module_dispatch(void *data, size_t data_size, void *target,
                                     size_t target_size) {
    (void)data;
    (void)data_size;
    (void)target;
    (void)target_size;

    const module_event_error_t event_error = {.error = MODULE_ERROR_NONE,
                                              .event = MODULE_EVENT_NONE};
    return event_error;
}

void *module_get_dispatch(void) { return (void *)&module_dispatch; }

module_error_e module_shutdown(void) { return MODULE_ERROR_NONE; }
