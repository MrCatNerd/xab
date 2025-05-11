#pragma once

#include "pch.h"
#include "shader.h"
#include "hashmap.h"

// is caching just communism?

// TODO: maybe tests?

// not very thread safe but i aint using multithreading

/**
 * @class ShaderItem
 * @brief a shader item for a shader hashmap cache
 *
 */
typedef struct ShaderItem {
        Shader_t shader;
        unsigned int refs;
} ShaderItem_t;

/**
 * @class ShaderCache
 * @brief a shader cache lol
 *
 */
typedef struct ShaderCache {
        struct hashmap *cache;
} ShaderCache_t;

/**
 * @brief Create a new shader cache
 *
 * @return shader cache
 */
ShaderCache_t create_shader_cache(void);

/**
 * @brief Resize a ShaderCache to a certain size
 * all new shaders are locked (accessing them with
 * shader_cache_create_or_cache_shader will unlock them)
 *
 * @param new_size - the new size, if the size is less than the current size
 * than all of the items outside the new size will be removed (ignores locking)
 * @param scache - shader cache
 */
void shader_cache_resize(unsigned int new_size, ShaderCache_t *scache);

/**
 * @brief Cache a shader in the cache, this will not check for duplicates, the
 * new cached shader will be with a ref count of 1, to set the refcount to zero
 * you can just unref the shader
 *
 * @param shader - the shader to cache
 * @param lock - whether to lock the shader node or not
 * @return the pointer to the shader in the shader cache
 */

Shader_t *shader_cache_add_to_cache(Shader_t shader, bool lock,
                                    ShaderCache_t *scache);

// very confusing name ik i suck at naming
/**
 * @brief Search for a shader in the cache return it if it exists, if it doesn't
 * exist then create a new shader in the cache and return it
 *
 * this function also unlocks the shader if it exists
 *
 * @param vertex_path - the vertex shader path
 * @param fragment_path - the fragment shader path
 * @param scache - shader cache
 * @return a shader pointer
 */
Shader_t *shader_cache_create_or_cache_shader(const char *vertex_path,
                                              const char *fragment_path,
                                              ShaderCache_t *scache);

/**
 * @brief Unref a specific shader, if there are 0 or less refs, the shader will
 * be deleted, unless it's locked
 *
 * @param shader - shader pointer
 * @param scache - shader cache
 *
 */
void shader_cache_unref_shader(Shader_t *shader, ShaderCache_t *scache);

/**
 * @brief Uncache/delete the shader from the shader cache
 *
 * @param snode - the shader node to delete
 * @param delete - whether to delete the (openGL) shader using the
 * 'delete_shader' function, this proves to be useful if you have another copy
 * of the OpenGL shader
 */
void shader_cache_uncache_shader(ShaderCache_t *scache,
                                 ShaderItem_t *shader_item, bool delete);

/**
 * @brief Cleanup theh shader cache, including the shaders themselves
 *
 * @param scache - shader cache
 */
void shader_cache_cleanup(ShaderCache_t *scache);
