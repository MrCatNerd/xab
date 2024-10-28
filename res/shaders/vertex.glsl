// vertex

#version 330 core

// position attribute index 0
layout(location = 0) in vec3 a_pos;

// uv attribute index 1
layout(location = 1) in vec2 a_uv;

// color attribute index 2
layout(location = 2) in vec3 a_color;

uniform mat4 ortho_proj;
uniform mat4 model;
uniform mat4 view;

out vec2 uv;

out vec3 color;

void main() {
    vec4 pos = vec4(a_pos.xy, 0, 1);
    gl_Position = ortho_proj * view * model * pos;

    uv = a_uv;

    color = a_color;
}
