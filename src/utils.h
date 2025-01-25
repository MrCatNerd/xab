#pragma once

#include "pch.h"
#include "length_string.h"

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
length_string_t ReadFile(const char *path);
