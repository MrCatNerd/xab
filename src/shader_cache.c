#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shader_cache.h"
#include "logger.h"
#include "shader.h"
#include "utils.h"

// TODO: find a way to print shorter filepaths (e.g. r/s/framebuffer_vertex.glsl
// instead of res/shaders/framebuffer_vertex.glsl) - put in utils.h

ShaderCache_t create_shader_cache(unsigned int reserve_size) {
    // just in case i mess up the calculation, totally didn't do that before
    if (reserve_size > 50)
        reserve_size = 5;
    else if (reserve_size < 1)
        reserve_size = 1;

    xab_log(LOG_DEBUG, "Creating a shader cache (%u shaders reserved)\n",
            reserve_size);

    ShaderCache_t scache = {{calloc(1, sizeof(ShaderNode_t))}, 0, 1};
    Assert(scache.shader_list.head != NULL);

    // reserve cache
    shader_cache_resize(reserve_size, &scache);
    return scache;
}

void shader_cache_resize(unsigned int new_size, ShaderCache_t *scache) {
    xab_log(LOG_TRACE, "Shader cache - resizing cache to size: %d\n", new_size);
    Assert(scache->shader_list.head != NULL);
    // get the end node
    ShaderNode_t *endnode = scache->shader_list.head;
    while (endnode->next != NULL) {
        endnode = endnode->next;
    }

    while (scache->realsize != new_size) {
        // if the target size is bigger then allocate a new node
        if (scache->realsize < new_size) {
            // allocate a new node
            endnode->next = calloc(1, sizeof(ShaderNode_t));
            Assert(endnode->next != NULL);

            // defaults + sanity checks
            endnode->next->refs = 0;
            endnode->next->lock = true;
            endnode->next->next = NULL;
            endnode->next->prev = endnode;

            // snode is now the next node
            endnode = endnode->next;

            scache->realsize++;

        }
        // if the target size is smaller then the size then delete the
        // shaders
        else if (scache->realsize > new_size) {
            // snode is the end of the list which means that if
            // scache->realsize-1 <= scache->used_size then you have to
            // delete the OpenGL shader
            if (scache->realsize - 1 <= scache->used_size)
                scache->used_size--;
            // delete is below

            // copy the address cuz imma overwrite it with the previous
            // node address
            ShaderNode_t *freenode = endnode;
            ShaderNode_t *prev = endnode->prev;

            shader_cache_uncache_shader(
                freenode, (scache->realsize - 1 <= scache->used_size));

            // snode is now the previous node
            endnode = prev;

            scache->realsize--;
        }
    }
}

Shader_t *shader_cache_create_or_cache_shader(const char *vertex_path,
                                              const char *fragment_path,
                                              ShaderCache_t *scache) {
    xab_log(LOG_TRACE,
            "Shader cache - searching for cached shader{vert: '%s', frag: "
            "'%s'}\n",
            vertex_path, fragment_path);

    ShaderNode_t *snode = scache->shader_list.head;
    Assert(snode != NULL);
    for (unsigned int i = 0; i < scache->used_size; i++) {
        Assert(snode != NULL);
        // if both of the paths are the same then return the shader
        if (!strcmp(snode->shader.paths[0], vertex_path) &&
            !strcmp(snode->shader.paths[1], fragment_path)) {
            // also add a reference to the shader and unlock it
            snode->refs++;
            snode->lock = false;

            xab_log(LOG_TRACE,
                    "Shader cache - returning cached shader{vert: '%s', frag: "
                    "'%s'}\n",
                    vertex_path, fragment_path);

            return &snode->shader;
        }

        snode = snode->next;
    }

    xab_log(LOG_TRACE,
            "Shader cache - creating, caching and returning shader{vert: '%s', "
            "frag: '%s'}\n",
            vertex_path, fragment_path);
    // create a new shader and cache it too, also return the shader (the pointer
    // points to the new cached shader address)
    Shader_t new_shader = create_shader(vertex_path, fragment_path);
    return shader_cache_add_to_cache(new_shader, false, scache);
}

Shader_t *shader_cache_add_to_cache(Shader_t shader, bool lock,
                                    ShaderCache_t *scache) {
    xab_log(LOG_VERBOSE,
            "Adding shader{vert: '%s', frag: '%s'} to shader cache\n",
            shader.paths[0], shader.paths[1]);
    ShaderNode_t *new_shader = NULL;

    if (scache->used_size < scache->realsize) {
        xab_log(LOG_TRACE,
                "Shader cache - using an unused shader node to add the "
                "shader{vert: '%s', frag: '%s'} to cache\n",
                shader.paths[0], shader.paths[1]);
        // if there are unused nodes, use them
        new_shader = scache->shader_list.head;
        for (unsigned int i = 0; i < scache->used_size; i++) {
            Assert(new_shader->next != NULL);
            new_shader = new_shader->next;
        }
    } else {
        xab_log(
            LOG_TRACE,
            "Shader cache - creating a new shader node to add the shader{vert: "
            "'%s', frag: '%s'} to cache\n",
            shader.paths[0], shader.paths[1]);
        // if there aren't just allocate a new node
        new_shader = calloc(1, sizeof(ShaderNode_t));
        Assert(new_shader != NULL);
        scache->realsize++;

        // now traverse to the highest used node and insert the new node
        ShaderNode_t *highest_used_snode = scache->shader_list.head;
        Assert(highest_used_snode != NULL);
        for (unsigned int i = 0; i < scache->used_size; i++) {
            highest_used_snode = highest_used_snode->next;
            Assert(highest_used_snode != NULL);
        }

        // and insert the new shader
        if (highest_used_snode->next != NULL) {
            new_shader->next = highest_used_snode->next;
            highest_used_snode->next->prev = new_shader;
        }
        new_shader->prev = highest_used_snode;
        if (highest_used_snode->next != NULL)
            highest_used_snode->next = new_shader; // order matters here
    }
    Assert(new_shader != NULL);

    new_shader->refs = 1;
    new_shader->shader = shader;
    new_shader->lock = lock;

    // increase the used size cuz we're using the shader
    scache->used_size++;

    return &new_shader->shader;
}

void shader_cache_unref_shader(Shader_t *shader, ShaderCache_t *scache) {
    // find the shader
    ShaderNode_t *snode = shader_cache_get_snode_from_shader(shader, scache);
    if (snode == NULL)
        xab_log(LOG_WARN, "Cannot unref shader{vert: '%s', frag: '%s'} because "
                          "it isn't cached\n");

    // decrease the refs by 1
    snode->refs--;
    xab_log(LOG_TRACE,
            "Unrefed shader{vert: '%s', frag: '%s'} - it has %d refs now \n",
            shader->paths[0], shader->paths[1], snode->refs);

    // if the refs are less or equal to 0, delete the shader
    if (snode->refs <= 0 && !snode->lock) {
        xab_log(LOG_VERBOSE,
                "Deleting shader{vert: '%s', frag: '%s'} from cache because it "
                "has %d refs left\n",
                shader->paths[0], shader->paths[1], snode->refs);

        scache->used_size--;
        shader_cache_uncache_shader(snode, true);
    }
}

ShaderNode_t *shader_cache_get_snode_from_shader(Shader_t *shader,
                                                 ShaderCache_t *scache) {
    // find the shader and return it
    Assert(scache->shader_list.head != NULL);
    ShaderNode_t *snode = scache->shader_list.head;

    // if there is no shader in the cache then snode will equal to NULL so it
    // will return NULL
    while (snode != NULL && shader->program_id != snode->shader.program_id) {
        snode = snode->next;
    }

    return snode;
}

void shader_cache_uncache_shader(ShaderNode_t *snode, bool delete) {
    if (snode->next != NULL)
        snode->next->prev = snode->prev;
    if (snode->prev != NULL)
        snode->prev->next = snode->next;

    if (delete)
        delete_shader(&snode->shader); // only deletes the opengl shader

    // if there are no next/previous nodes, than it means that it is the only
    // node left, if this happens just reuse this node as the head node
    if (!snode->prev && !snode->next) {
        snode->lock = true;
        snode->refs = 0;
        snode->shader.paths[0] = "";
        snode->shader.paths[1] = "";
        snode->shader.program_id = 0;
        // sanity checks
        snode->prev = NULL;
        snode->next = NULL;
    } else
        free(snode);
}

bool shader_cache_set_lock_shader(Shader_t *shader, ShaderCache_t *scache,
                                  bool lock) {
    ShaderNode_t *snode = shader_cache_get_snode_from_shader(shader, scache);
    if (snode == NULL)
        return false;

    snode->lock = lock;

    return true;
}

void shader_cache_cleanup(ShaderCache_t *scache) {
    xab_log(LOG_DEBUG, "Cleaning up shader cache\n");

    // traverse to the end and clean from there
    ShaderNode_t *endnode = scache->shader_list.head;
    Assert(endnode != NULL);

    while (endnode->next != NULL) {
        endnode = endnode->next;
    }

    while (endnode != NULL) {
        xab_log(LOG_TRACE,
                "Shader cache cleanup - cleaning shader node: #%d{%s, %s} (%d "
                "refs)\n",
                endnode->shader.program_id, endnode->shader.paths[0],
                endnode->shader.paths[1], endnode->refs);

        // copy the address cuz imma overwrite it with the previous node
        // address
        ShaderNode_t *freenode = endnode;

        // change the endnode to the previous node
        endnode = endnode->prev;

        // now delete and free the node
        shader_cache_uncache_shader(freenode, true);
    }

    scache->used_size = 0;
    scache->realsize = 0;
}
