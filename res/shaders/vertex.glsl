// vertex

#version 450 core

// position attribute index 0
layout(location = 0) in vec3 a_pos;

// uv attribute index 1
layout(location = 1) in vec2 a_uv;

// color attribute index 2
layout(location = 2) in vec3 a_color;

// (from ARB_explicit_uniform_location)
// matrix uniform location 0
layout(location = 0) uniform mat3 u_matrix;

// required because of ARB_separate_shader_objects
out gl_PerVertex {
    vec4 gl_Position;
};

out vec2 uv;

out vec4 color;

void main() {
    vec4 pos = vec4(u_matrix * a_pos, 1.0);

    gl_Position = vec4(pos.xy, 0, 1);

    uv = a_uv;

    color = vec4(a_color.rgb, 1);
};
