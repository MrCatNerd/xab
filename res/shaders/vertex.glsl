// vertex

#version 450 core

// position attribute index 0
layout(location = 0) in vec3 a_pos;

// uv attribute index 1
layout(location = 1) in vec2 a_uv;

// color attribute index 2
layout(location = 2) in vec3 a_color;

uniform mat3 rot_mat;
uniform mat3 shear_mat;

out vec2 uv;

out vec3 color;

void main() {
    vec3 pos = shear_mat * a_pos;
    gl_Position = vec4(pos.xy, 0, 1);

    uv = a_uv;

    color = a_color;
}
