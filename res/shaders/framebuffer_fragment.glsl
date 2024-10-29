// fragment

#version 330 core
out vec4 FragColor;

in vec2 uv;

uniform sampler2D screenTexture;
uniform float Time;

const float offset_x = 1.0f / 3840.0f;
const float offset_y = 1.0f / 2160.0f;

vec2 offsets[9] = vec2[](
        vec2(-offset_x, offset_y), vec2(0.0, offset_y), vec2(offset_x, offset_y),
        vec2(-offset_x, 0.0), vec2(0.0, 0.0), vec2(offset_x, 0.0),
        vec2(-offset_x, -offset_y), vec2(0.0, -offset_y), vec2(offset_x, offset_y)
    );

float kernel[9] = float[](
        -1, -1, -1,
        -1, 8, -1,
        -1, -1, -1
    );

void main()
{
    vec3 color = vec3(texture(screenTexture, uv));

    // vec3 color = vec3(0.0f);
    // for (int i = 0; i < 9; i++)
    //     color +=vec3(texture(screenTexture, uv.st + offsets[i])) * kernel[i];
    // if (max(color-0.05, 0) == vec3(0.0))
    //     color = vec3(0.5098, 0.7137, 0.8509);

    // vec3 color = vec3(texture(screenTexture, vec2(uv.x + sin(Time * 0.5 + uv.y * 3) * 0.2, uv.y))); // trippy wallpaper lol
    FragColor = vec4(color.rgb, 1.0f);
}
