// fragment

#version 330 core

precision mediump float;

in vec2 uv;

out vec4 FragColor;

uniform sampler2D u_wallpaperTexture;
uniform int u_flip_y;

void main()
{
    vec3 color = vec3(texture(u_wallpaperTexture, vec2(uv.x, u_flip_y - uv.y)));

    FragColor = vec4(color.rgb, 1.0f);
}
