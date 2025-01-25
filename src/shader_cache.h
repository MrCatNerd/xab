#pragma once

#include "pch.h"
#include "shader.h"

// is caching just communism?

// TODO: maybe tests?

// not very thread safe but i aint using multithreading

/**
 * @class ShaderNode
 * @brief shader node for a shader linked list with extra data
 *
 */
typedef struct ShaderNode {
        Shader_t shader;
        unsigned int refs;
        /**
         * @brief if the node is locked, than when it won't be deleted on 0 refs
         */
        bool lock;
        struct ShaderNode *next;
        struct ShaderNode *prev;
} ShaderNode_t;

/**
 * @class LinkedShaderList
 * @brief a linked list for shaders shader
 *
 */
typedef struct LinkedShaderList {
        ShaderNode_t *head;
} LinkedShaderList_t;

/**
 * @class ShaderCache
 * @brief shader cache
 *
 */
typedef struct ShaderCache {
        /**
         * @brief Shader linked list - the cache
         */
        LinkedShaderList_t shader_list;

        /**
         * @brief The size of the used shaders (the ones that are not empty)
         */
        unsigned int used_size;

        /**
         * @brief The actual size of the allocated shaders (empty ones included)
         */
        unsigned int realsize;
} ShaderCache_t;

/**
 * @brief Create a new shader cache
 *
 * @param reserve_size how much shaders to reserve
 * @return shader cache
 */
ShaderCache_t create_shader_cache(unsigned int reserve_size);

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
 * exist than create a new shader in the cache and return it
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
 * @brief Get the shader's shader node from the shader cache if it has a cache,
 * if it doesn't than this will return NULL
 *
 * @param shader - the shader
 * @param scache - the shader cache
 * @return the shader node
 */
ShaderNode_t *shader_cache_get_snode_from_shader(Shader_t *shader,
                                                 ShaderCache_t *scache);

/**
 * @brief Uncache/delete the shader from the shader cache, this function also
 * frees the node (and the shader that belongs to the node)
 *
 * @param snode - the shader node to delete
 * @param delete - whether to delete the (openGL) shader using the
 * 'delete_shader' function, this can be useful if you have another copy of the
 * shader, or if the node is unused
 */
void shader_cache_uncache_shader(ShaderNode_t *snode, bool delete);

/**
 * @brief Lock/Unlock the shader in the cache (this is just a wrapper for
 * `shader_cache_snode_from_shader`)
 *
 * @param shader - a shader pointer
 * @param scache - a shader cache
 * @param lock - yes
 * @return success - false if the shader isn't in the cache
 */
bool shader_cache_set_lock_shader(Shader_t *shader, ShaderCache_t *scache,
                                  bool lock);

/**
 * @brief Cleanup theh shader cache, including the shaders themselves
 *
 * @param scache - shader cache
 */
void shader_cache_cleanup(ShaderCache_t *scache);
