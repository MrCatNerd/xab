// vertex

#version 330 core

precision mediump float;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 uv;

uniform mat4 ortho_proj;
uniform mat4 model;
uniform mat4 view;

void main()
{
    vec4 pos = vec4(aPos.xy, 0.0f, 1.0f);
    gl_Position = ortho_proj * view * model * pos;
    uv = aTexCoords;
}
