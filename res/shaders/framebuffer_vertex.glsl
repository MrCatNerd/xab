// vertex

#version 330 core

precision mediump float;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;

out vec2 uv;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0f, 1.0f);
    uv = aTexCoords;
}
