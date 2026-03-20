#ifndef __ISP_H__
#define __ISP_H__

#include "rpi.h"
#include "frame.h"

// sRGB gamma LUT: maps linear 0-255 to gamma-corrected 0-255.
// Computed offline by claude: sRGB(x) = 1.055*x^(1/2.4) - 0.055 for x > 0.0031308
// Computations taken from the following wikipedia page: https://en.wikipedia.org/wiki/SRGB
static const uint8_t gamma_lut[256] = {
      0,  13,  22,  28,  34,  38,  42,  46,  50,  53,  56,  59,  61,  64,  66,  69,
     71,  73,  75,  77,  79,  81,  83,  85,  86,  88,  90,  92,  93,  95,  96,  98,
     99, 101, 102, 104, 105, 106, 108, 109, 110, 112, 113, 114, 115, 117, 118, 119,
    120, 121, 122, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
    137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 148, 149, 150, 151,
    152, 153, 154, 155, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 163, 164,
    165, 166, 167, 167, 168, 169, 170, 170, 171, 172, 173, 173, 174, 175, 175, 176,
    177, 178, 178, 179, 180, 180, 181, 182, 182, 183, 184, 185, 185, 186, 187, 187,
    188, 189, 189, 190, 190, 191, 192, 192, 193, 194, 194, 195, 196, 196, 197, 197,
    198, 199, 199, 200, 200, 201, 202, 202, 203, 203, 204, 205, 205, 206, 206, 207,
    208, 208, 209, 209, 210, 210, 211, 212, 212, 213, 213, 214, 214, 215, 215, 216,
    216, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224, 224,
    225, 226, 226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233,
    233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 238, 238, 239, 239, 240, 240,
    241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246, 246, 247, 247, 248,
    248, 249, 249, 250, 250, 251, 251, 251, 252, 252, 253, 253, 254, 254, 255, 255,
};

// unpack one pixel from camera's RAW12 buffer
static inline uint32_t unpack_raw12(volatile uint8_t *buf,
                                    uint32_t x, uint32_t y)
{
    uint32_t pair_off = (x >> 1) * 3;
    uint32_t off = y * FRAME_LINE_STRIDE + pair_off;
    uint8_t b0 = buf[off];
    uint8_t b1 = buf[off + 1];
    uint8_t b2 = buf[off + 2];
    if (x & 1)              // b0 -- b2 encode 2 pixels, so check if odd or even before returning
        return ((uint32_t)b1 << 4) | ((b2 >> 4) & 0x0F);
    else
        return ((uint32_t)b0 << 4) | (b2 & 0x0F);
}

static inline uint8_t process_channel(uint32_t sum, uint32_t cnt,
                                      uint16_t wb_gain)
{
    if (cnt == 0) return 0;
    uint32_t avg = sum / cnt;

    int32_t val = (int32_t)avg - 256;
    if (val < 0) val = 0;

    if (val > 3839) val = 3839;
    val = (uint32_t)val * 255 / 3839;

    val = ((uint32_t)val * wb_gain) >> 8;
    if (val > 255) val = 255;

    return gamma_lut[val];
}

#endif
