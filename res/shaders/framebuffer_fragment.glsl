// fragment

#version 330 core

precision mediump float;

in vec2 uv;

out vec4 FragColor;

uniform sampler2D u_screenTexture;
uniform float u_Time;

const float offset_x = 1.0f / 3840.0f;
const float offset_y = 1.0f / 2160.0f;

vec2 offsets[9] = vec2[](
        vec2(-offset_x, offset_y), vec2(0.0f, offset_y), vec2(offset_x, offset_y),
        vec2(-offset_x, 0.0f), vec2(0.0f, 0.0f), vec2(offset_x, 0.0f),
        vec2(-offset_x, -offset_y), vec2(0.0f, -offset_y), vec2(offset_x, offset_y)
    );

float kernel[9] = float[](
        -1, -1, -1,
        -1, 8, -1,
        -1, -1, -1
    );

void main()
{
    vec3 color = vec3(texture(u_screenTexture, uv));

    // vec3 color = vec3(0.0f);
    // for (int i = 0; i < 9; i++)
    //     color += vec3(texture(u_screenTexture, uv.st + offsets[i])) * kernel[i];
    // if (max(color - 0.05f, 0.0f) == vec3(0.0f))
    //     color = vec3(0.5098f, 0.7137f, 0.8509f);

    // vec3 color = vec3(texture(u_screenTexture, vec2(uv.x + sin(u_Time * 0.5 + uv.y * 3) * 0.2, uv.y))); // trippy wallpaper lol
    FragColor = vec4(color.rgb, 1.0f);
}
