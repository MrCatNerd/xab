// vertex

#version 330 core

precision mediump float;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 uv;

uniform mat4 u_ortho_proj;
uniform mat4 u_view;
uniform mat4 u_model;

void main()
{
    vec4 pos = vec4(aPos.xy, 0.0f, 1.0f);
    gl_Position = u_ortho_proj * u_view * u_model * pos;
    uv = aTexCoords;
}
