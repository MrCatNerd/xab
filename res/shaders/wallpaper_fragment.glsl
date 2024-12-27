// fragment

#version 330 core

precision mediump float;

in vec2 uv;

out vec4 FragColor;

uniform sampler2D wallpaperTexture;
uniform int flip_y;

void main()
{
    vec3 color = vec3(texture(wallpaperTexture, vec2(uv.x, flip_y - uv.y)));

    FragColor = vec4(color.rgb, 1.0f);
}
