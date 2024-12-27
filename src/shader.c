#include <stdlib.h>
#include <epoxy/gl.h>

#include "shader.h"
#include "logger.h"
#include "utils.h"

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
    const char *vshader_src = ReadFile(vertex_path);
    vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vshader, 1, &vshader_src, NULL);
    glCompileShader(vshader);

    glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vshader, sizeof(infoLog), NULL, infoLog);
        xab_log(LOG_ERROR, "Failed to compile vertex shader: '%s'\n%s\n",
                vertex_path, infoLog);
    }

    // fragment shader
    unsigned int fshader;
    const char *fshader_src = ReadFile(fragment_path);
    fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fshader, 1, &fshader_src, NULL);
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

    free((void *)vshader_src);
    free((void *)fshader_src);

    return shader;
}

void delete_shader(Shader_t *shader) { glDeleteProgram(shader->program_id); }
