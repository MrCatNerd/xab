#pragma once

#include <assert.h>

// replace this with your favorite Assert() implementation
#define Assert(cond) assert(cond)

#define STR(x) #x

/**
 * @brief Read a file from path
 *
 * you need to manually free the buffer
 *
 * @param path
 * @param buffer
 */
const char *ReadFile(const char *path);
