// fragment

#version 330 core

precision mediump float;

in vec2 uv;

out vec4 FragColor;

uniform sampler2D u_wallpaperTextureY;
uniform sampler2D u_wallpaperTextureU;
uniform sampler2D u_wallpaperTextureV;
// HDTV - basically sRGB equivilent
#define BT709 0
// SDTV
#define BT601 1
// UHDTV
#define BT2020 2
uniform int u_colorspace;
// full range
#define JPEG_RANGE 0
// limited range
#define MPEG_RANGE 1
uniform int u_colorrange;

uniform int u_flip_y;

// based on: https://en.wikipedia.org/wiki/Y%E2%80%B2UV
const mat3 bt601_to_rgb_matrix = mat3(
    1,  0      ,  1.13983,
    1, -0.39465, -0.58060,
    1,  2.03211,  0
);

// based on: https://en.wikipedia.org/wiki/Y%E2%80%B2UV
const mat3 bt709_to_rgb_matrix = mat3(
    1,  0      ,  1.28033,
    1, -0.21482, -0.38059,
    1,  2.12798,  0
);

// based on: https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.2407-2017-PDF-E.pdf#%5B%7B%22num%22%3A37%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C54%2C525%2C0%5D
const mat3 bt2020_to_bt709_matrx = mat3(
     1.6605, -0.5876, -0.0728,
    -0.1246,  1.1329, -0.0083,
    -0.0182, -0.1006,  1.1187
);

// too lazy to hardcode it
const mat3 bt2020_to_rgb_matrix = bt2020_to_bt709_matrx * bt709_to_rgb_matrix;

mat3 get_color_matrix(){
    switch (u_colorspace){
        case BT601:
        return bt601_to_rgb_matrix;
        break;
        case BT709:
        return bt709_to_rgb_matrix;
        break;
        case BT2020:
        return bt2020_to_rgb_matrix;
        break;
    }
    return mat3(1.0);
}

// based on: https://trac.ffmpeg.org/wiki/colorspace
#define MPEG_LUMA_MIN   (16)
#define MPEG_CHROMA_MIN (16)
#define MPEG_LUMA_MAX   (235)
#define MPEG_CHROMA_MAX (240)
#define JPEG_LUMA_MIN   (0)
#define JPEG_CHROMA_MIN (1)
#define JPEG_LUMA_MAX   (255)
#define JPEG_CHROMA_MAX (255)
vec3 convert_to_jpeg_color_range(vec3 color) {
    // source: libavutil/pixfmt.h (enum AVColorRange)
    /**
     * Visual content value range.
     *
     * These values are based on definitions that can be found in multiple
     * specifications, such as ITU-T BT.709 (3.4 - Quantization of RGB, luminance
     * and colour-difference signals), ITU-T BT.2020 (Table 5 - Digital
     * Representation) as well as ITU-T BT.2100 (Table 9 - Digital 10- and 12-bit
     * integer representation). At the time of writing, the BT.2100 one is
     * recommended, as it also defines the full range representation.
     *
     * Common definitions:
     *   - For RGB and luma planes such as Y in YCbCr and I in ICtCp,
     *     'E' is the original value in range of 0.0 to 1.0.
     *   - For chroma planes such as Cb,Cr and Ct,Cp, 'E' is the original
     *     value in range of -0.5 to 0.5.
     *   - 'n' is the output bit depth.
     *   - For additional definitions such as rounding and clipping to valid n
     *     bit unsigned integer range, please refer to BT.2100 (Table 9).
     */

    switch (u_colorrange){
        /**
         * Full range content.
         *
         * - For RGB and luma planes:
         *
         *       (2^n - 1) * E
         *
         *   F.ex. the range of 0-255 for 8 bits
         *
         * - For chroma planes:
         *
         *       (2^n - 1) * E + 2^(n - 1)
         *
         *   F.ex. the range of 1-255 for 8 bits
         */
        case JPEG_RANGE:
        return color;
        break;
        /**
         * Narrow or limited range content.
         *
         * - For luma planes:
         *
         *       (219 * E + 16) * 2^(n-8)
         *
         *   F.ex. the range of 16-235 for 8 bits
         *
         * - For chroma planes:
         *
         *       (224 * E + 128) * 2^(n-8)
         *
         *   F.ex. the range of 16-240 for 8 bits
         */
        case MPEG_RANGE:
        // TODO: this
        return color;
        break;
    }
}

void main()
{
    vec3 YCbCr = vec3(
        texture(u_wallpaperTextureY, vec2(uv.x, u_flip_y - uv.y)).r, // Luma
        texture(u_wallpaperTextureU, vec2(uv.x, u_flip_y - uv.y)).r, // Cb
        texture(u_wallpaperTextureV, vec2(uv.x, u_flip_y - uv.y)).r  // Cr
    );

    FragColor = vec4(convert_to_jpeg_color_range(YCbCr * get_color_matrix()), 1.0);
}
