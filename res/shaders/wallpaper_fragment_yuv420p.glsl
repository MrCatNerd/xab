// fragment

#version 330 core

precision mediump float;

in vec2 uv;

out vec4 FragColor;

uniform sampler2D u_wallpaperTextureY;
uniform sampler2D u_wallpaperTextureU;
uniform sampler2D u_wallpaperTextureV;
uniform int u_flip_y;

void main()
{
    float r, g, b, y, u, v;

    y = texture(sTextureY, vec2(uv.x, u_flip_y - uv.y)).r;
    u = texture(sTextureU, vec2(uv.x, u_flip_y - uv.y)).r;
    v = texture(sTextureV, vec2(uv.x, u_flip_y - uv.y)).r;

    y = 1.1643*(y-0.0625);
    u = u-0.5;
    v = v-0.5;

    r = y+1.5958*v;
    g = y-0.39173*u-0.81290*v;
    b = y+2.017*u;

    FragColor = vec4(r, g, b, 1.0f);
}
