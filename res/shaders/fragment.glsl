// fragment

#version 330 core

in vec2 uv;
in vec3 color;

uniform sampler2D s_texture;
uniform float Time;

// output fragment data location 0
layout(location = 0) out vec4 o_color;

void main() {
    // vec3 raw = vec3(texture(s_texture, vec2(uv.x, 1 - uv.y)));
    vec3 raw = vec3(texture(s_texture, vec2(uv.x + sin(Time*1.5 + uv.y*15)*0.1, 1-uv.y)));

    // vec3 result = color * raw;
    vec3 result = raw;


    o_color = vec4(result.rgb, 1.0);
}
