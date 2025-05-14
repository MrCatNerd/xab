#pragma once

#include <stddef.h>
#include "module_contract_types.h"

/// module contract version stuff
unsigned int module_get_contract_version(void);

/// module version stuff
const char *module_get_name(void);
int module_get_version_major(void);
int module_get_version_minor(void);
int module_get_version_patch(void);

// --- data and resources --- //

/// returns an error string, or NULL on success
const char *module_verify_data(void *data, unsigned long size);

/// returns a list of empty data types used to fill the `target` pointer in
/// dispatch, the `types` pointer inside the returned struct must be
/// heap-allocated and will be freed by the engine in addition,
/// the `len`/`types` members of the returned struct can be set te 0/NULL to
/// specify that the dispatch function shouldn't set the `target` pointer
module_typelist_t module_get_types(void);

/// returns a list of filled data types used to fill the `data` pointer in
/// dispatch, this function shall only return types with the
/// MODULE_TYPE_xxx_CONTEXT suffix. the `types` pointer inside the returned
/// struct must be heap-allocated and will be freed by the engine in addition,
/// the `len`/`types` members of the returned struct can be set te 0/NULL to
/// specify that the dispatch function shouldn't set the `data` pointer
module_typelist_t module_resource_requirements(void);

// --- callbacks and stuff --- //

/// will be called when the module is initialized
module_error_e module_init(void);

/// dispatch entry point, called each frame
/// returns an event + error struct
module_event_error_t module_dispatch(void *data, size_t data_size, void *target,
                                     size_t target_size);

/// returns a pointer to the dispatch function, usefull if you are planning to
/// dynamically switch dispatch functions
void *module_get_dispatch(void);

/// will be called when the module is freed
module_error_e module_shutdown(void);
