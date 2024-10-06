#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{
    FragColor = vec4(vec3(0.5f), 1.0f) - vec4(texture(screenTexture, TexCoords));
}

