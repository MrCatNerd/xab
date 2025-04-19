#pragma once

typedef struct Shader {
        /// OpenGL shader program ID
        unsigned int program_id;

        /// paths of the shader files - vertex and than fragment
        const char *paths[2];
} Shader_t;

Shader_t create_shader(const char *vertex_path, const char *fragment_path);

void use_shader(Shader_t *shader);

// i think caching uniform locations is a little bit overkill for xab at the
// moment
int shader_get_uniform_location(Shader_t *shader, const char *name);

void delete_shader(Shader_t *shader);
