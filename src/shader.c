#include <stdlib.h>
#include <string.h>
#include <epoxy/gl.h>

#include "shader.h"
#include "logger.h"
#include "utils.h"

#include "length_string.h"

#ifndef DISABLE_BCE
#include "bce_files.h"
#endif

// very spaghett
static length_string_t search_bce_shaders(const char *path);
typedef struct free_file {
        length_string_t lenstr;
        bool should_free;
} free_file_t;

static free_file_t get_shader_file(const char *path) {
    xab_log(LOG_TRACE, "Getting shader file: %s\n", path);
#ifndef DISABLE_BCE
    free_file_t shader_file = {{NULL, NULL}, false};

    shader_file.lenstr = search_bce_shaders(path);

    if (shader_file.lenstr.str == NULL) {
        shader_file.lenstr = ReadFile(path);
        shader_file.should_free = true;
    }

#else
    free_file_t shader_file = {ReadFile(path), true};
#endif

    return shader_file;
}

#ifndef DISABLE_BCE
static length_string_t search_bce_shaders(const char *path) {
    xab_log(LOG_TRACE, "Searching BCE files...\n");
    // haha no hashmaps too lazy

    for (int i = 0; i < (int)bce_files_len; i++) {
        bce_file_t bce_file = bce_files[i];
        if (strcmp(bce_file.path, path) == 0) {
            xab_log(LOG_TRACE, "BCE file `%s` was found\n", bce_file.path);
            Assert(bce_file.contents.len >= 0);

            return bce_file.contents;
        }
    }

    xab_log(LOG_TRACE, "no BCE files found, reading files...\n");

    const length_string_t empty = {NULL, 0};
    return empty;
}
#endif

int shader_get_uniform_location(Shader_t *shader, const char *name) {
    return glGetUniformLocation(shader->program_id, name);
}

void use_shader(Shader_t *shader) { glUseProgram(shader->program_id); }

Shader_t create_shader(const char *vertex_path, const char *fragment_path) {
    xab_log(LOG_DEBUG,
            "Creating shader from '%s' (vertex) and '%s' (fragment)\n",
            vertex_path, fragment_path);
    Shader_t shader = {0};

    // error stuff
    int success;
    char infoLog[1024];

    // vertex shader
    unsigned int vshader;
    free_file_t vshader_src = get_shader_file(vertex_path);
    vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 1, (const GLchar *const *)&vshader_src.lenstr.str,
                   (const GLint *)&vshader_src.lenstr.len);
    glCompileShader(vshader);

    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vshader, sizeof(infoLog), NULL, infoLog);
        xab_log(LOG_ERROR, "Failed to compile vertex shader: '%s'\n%s\n",
                vertex_path, infoLog);
    }

    // fragment shader
    unsigned int fshader;
    free_file_t fshader_src = get_shader_file(fragment_path);
    fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, 1, (const GLchar *const *)&fshader_src.lenstr.str,
                   (const GLint *)&fshader_src.lenstr.len);
    glCompileShader(fshader);

    glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fshader, sizeof(infoLog), NULL, infoLog);
        xab_log(LOG_ERROR, "Failed to compile fragment shader: '%s'\n%s\n",
                fragment_path, infoLog);
    }

    // shader program
    shader.program_id = glCreateProgram();
    glAttachShader(shader.program_id, vshader);
    glAttachShader(shader.program_id, fshader);
    glLinkProgram(shader.program_id);

    glGetProgramiv(shader.program_id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader.program_id, sizeof(infoLog), NULL, infoLog);
        xab_log(LOG_ERROR, "Failed to link shaders!\n%s\n", infoLog);
    }

    // cleanup (doesn't cleanup the the program itself)
    glDeleteShader(vshader);
    glDeleteShader(fshader);

    if (vshader_src.should_free)
        free((void *)vshader_src.lenstr.str);
    if (fshader_src.should_free)
        free((void *)fshader_src.lenstr.str);

    return shader;
}

void delete_shader(Shader_t *shader) { glDeleteProgram(shader->program_id); }
