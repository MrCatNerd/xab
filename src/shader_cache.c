#include "pch.h"
#include "hashmap.h"

#include "shader_cache.h"
#include "logger.h"
#include "shader.h"
#include "utils.h"

// TODO: find a way to print shorter filepaths (e.g. r/s/framebuffer_vertex.glsl
// instead of res/shaders/framebuffer_vertex.glsl) - put in utils.h

static int shader_cache_compare(const void *a, const void *b, void *udata) {
    (void)(udata);
    const ShaderItem_t *atom_item_a = (const ShaderItem_t *)a;
    const ShaderItem_t *atom_item_b = (const ShaderItem_t *)b;
    return strcmp(atom_item_a->shader.paths[0], atom_item_b->shader.paths[0]) &&
           strcmp(atom_item_a->shader.paths[1], atom_item_b->shader.paths[1]);
}

static uint64_t shader_cache_hash(const void *item, uint64_t seed0,
                                  uint64_t seed1) {
    const ShaderItem_t *atom_item = (const ShaderItem_t *)item;

    // vertex
    const char *vert_str = atom_item->shader.paths[0];
    const unsigned int vert_len = strlen(vert_str);

    // fragment
    const char *frag_str = atom_item->shader.paths[1];
    const unsigned int frag_len = strlen(frag_str);

    // hash them both seperately with murmur for speeeeed
    const uint64_t vert_hash = hashmap_murmur(vert_str, vert_len, seed0, seed1);
    const uint64_t frag_hash = hashmap_murmur(frag_str, frag_len, seed0, seed1);

    // combine them two in a list and hash the combined list with sip cuz its
    // better for some reason
    const uint64_t combined_hashes[2] = {vert_hash, frag_hash};

    // all of the hard work to avoid one malloc lol
    return hashmap_sip(&combined_hashes, sizeof(combined_hashes), seed0, seed1);
}

ShaderCache_t create_shader_cache(void) {
    xab_log(LOG_DEBUG, "Creating a shader cache\n");

    ShaderCache_t scache = {
        .cache = hashmap_new(sizeof(ShaderItem_t), 0, 0, 0, shader_cache_hash,
                             shader_cache_compare, NULL, NULL)};
    Assert(scache.cache != NULL);

    return scache;
}

Shader_t *shader_cache_create_or_cache_shader(const char *vertex_path,
                                              const char *fragment_path,
                                              ShaderCache_t *scache) {
    xab_log(LOG_TRACE,
            "Shader cache: searching for shader{vert: '%s', frag: "
            "'%s'} in cache\n",
            vertex_path, fragment_path);

    // seacrh for the shader in the cache
    ShaderItem_t *shader_item = (ShaderItem_t *)hashmap_get(
        scache->cache,
        &(ShaderItem_t){.shader.paths = {vertex_path, fragment_path}});
    if (shader_item) {
        // if its found then raise the refs and return it
        xab_log(LOG_TRACE,
                "Shader cache: shader{vert: '%s', frag: '%s'} found in cache\n",
                vertex_path, fragment_path);

        shader_item->refs++;
        return &shader_item->shader;
    }

    // if not then create a new shader and cache it with a ref count of 1, and
    // then return it
    xab_log(LOG_TRACE,
            "Shader cache: shader{vert: '%s', "
            "frag: '%s'} not found in cache, creating and caching it\n",
            vertex_path, fragment_path);

    Shader_t new_shader = create_shader(vertex_path, fragment_path);
    hashmap_set(scache->cache,
                &(ShaderItem_t){.shader = new_shader, .refs = 1});
    shader_item = (ShaderItem_t *)hashmap_get(
        scache->cache,
        &(ShaderItem_t){.shader.paths = {vertex_path, fragment_path}});
    Assert(shader_item != NULL &&
           "Shader is NULL for some strange reason, rip");

    return &shader_item->shader;
}

void shader_cache_unref_shader(Shader_t *shader, ShaderCache_t *scache) {
    // find the shader
    ShaderItem_t *shader_item = (ShaderItem_t *)hashmap_get(
        scache->cache,
        &(ShaderItem_t){.shader.paths = {shader->paths[0], shader->paths[1]}});

    if (shader_item == NULL)
        xab_log(LOG_WARN, "Shader cache: Cannot unref shader{vert: '%s', frag: "
                          "'%s'} because it isn't cached\n");

    // decrease the refs by 1
    shader_item->refs--;
    xab_log(LOG_TRACE,
            "Shader cache: Unrefed shader{vert: '%s', frag: '%s'}, it has %d "
            "refs now \n",
            shader->paths[0], shader->paths[1], shader_item->refs);

    // if the refs are less or equal to 0, delete the shader
    if (shader_item->refs <= 0)
        shader_cache_uncache_shader(scache, shader_item, true);
}

void shader_cache_uncache_shader(ShaderCache_t *scache,
                                 ShaderItem_t *shader_item, bool delete) {
    if (!scache || !shader_item)
        return;

    xab_log(LOG_TRACE,
            "Uncaching shader{vert: '%s', frag: '%s'} with %d refs\n",
            shader_item->shader.paths[0], shader_item->shader.paths[1],
            shader_item->refs);

    if (delete)
        delete_shader(&shader_item->shader); // only deletes the opengl shader

    hashmap_delete(scache->cache, shader_item);
}

void shader_cache_cleanup(ShaderCache_t *scache) {
    xab_log(LOG_DEBUG, "Cleaning up shader cache\n");

    ShaderItem_t *shader_item = NULL;
    size_t i = 0;
    while (hashmap_iter(scache->cache, &i, (void **)&shader_item)) {
        if (shader_item == NULL) // justin case
            break;

        xab_log(LOG_TRACE,
                "Shader cache: Cleaning up shader{vert: '%s', frag: '%s'} with "
                "%d refs (OpenGL only)\n",
                shader_item->shader.paths[0], shader_item->shader.paths[1],
                shader_item->refs);

        // delete only the OpenGL shader
        delete_shader(&shader_item->shader);
    }

    // now actually free delete the hashmap
    hashmap_free(scache->cache);
    scache->cache = NULL;
}
