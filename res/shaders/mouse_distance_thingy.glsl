#version 330 core
// i got distracted...

precision mediump float;

in vec2 uv;

out vec4 FragColor;

// video stuff
uniform sampler2D u_wallpaperTexture;
uniform int u_flip_y;

// mouse stuff
uniform vec2 u_resolution;
uniform vec2 u_mouse_pixel_pos;
uniform vec3 u_mouse_color;

void main() {
    // todo: normalize distance
    vec3 color_distance_to_mouse = mouse_color * (distance(glFragcoord.xy, u_mouse_pixel_pos) / u_resolution);
    vec3 color_vid = vec3(texture(u_wallpaperTexture, vec2(uv.x, u_flip_y - uv.y)));
    vec3 color = color_vid * color_distance;

    FragColor = vec4(color.rgb, 1.0f);
}
