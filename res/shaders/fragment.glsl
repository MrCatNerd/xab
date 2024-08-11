// fragment

#version 450 core

in vec2 uv;
in vec4 color;

// (from ARB_shading_language_420pack)
// texture unit binding 0
layout(binding = 0) uniform sampler2D s_texture;

// output fragment data location 0
layout(location = 0) out vec4 o_color;

void main() {
    o_color = color * texture(s_texture, uv);
};
