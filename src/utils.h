#pragma once

#include "pch.h"
#include "length_string.h"

#define FUNCTION_PTR(type, name, value, ...)                                   \
    type (*name)(__VA_ARGS__) = (type (*)(__VA_ARGS__))(value)

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
