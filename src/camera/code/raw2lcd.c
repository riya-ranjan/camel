#include "raw2lcd.h"
#include "ili9486.h"

// sRGB gamma LUT: maps linear 0-255 to gamma-corrected 0-255.
// Computed offline from: sRGB(x) = 1.055*x^(1/2.4) - 0.055 for x > 0.0031308.
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

// Unpack one 12-bit pixel from MIPI CSI-2 packed RAW12 buffer.
// Every 3 bytes encode 2 pixels: byte0=pix0[11:4], byte1=pix1[11:4],
// byte2=pix0[3:0]|pix1[7:4].
static inline uint32_t unpack_raw12(volatile uint8_t *buf,
                                    uint32_t x, uint32_t y)
{
    uint32_t pair_off = (x >> 1) * 3;
    uint32_t off = y * FRAME_LINE_STRIDE + pair_off;
    uint8_t b0 = buf[off];
    uint8_t b1 = buf[off + 1];
    uint8_t b2 = buf[off + 2];
    if (x & 1)
        return ((uint32_t)b1 << 4) | ((b2 >> 4) & 0x0F);
    else
        return ((uint32_t)b0 << 4) | (b2 & 0x0F);
}

// Process one color channel: average, black-level subtract, normalize,
// apply WB gain, gamma correct.
static inline uint8_t process_channel(uint32_t sum, uint32_t cnt,
                                      uint16_t wb_gain)
{
    if (cnt == 0) return 0;
    uint32_t avg = sum / cnt;

    // Black level subtract (IMX477 black level = 256 in 12-bit).
    int32_t val = (int32_t)avg - 256;
    if (val < 0) val = 0;

    // Normalize from 0-3839 to 0-255.
    if (val > 3839) val = 3839;
    val = (uint32_t)val * 255 / 3839;

    // Apply WB gain (Q8.8 fixed-point).
    val = ((uint32_t)val * wb_gain) >> 8;
    if (val > 255) val = 255;

    return gamma_lut[val];
}

void raw2lcd_init(void)
{
    // Set landscape mode: 480 wide x 320 tall.
    ili9486_set_rotation(MADCTL_MX | MADCTL_MV);
}

void raw2lcd_display(frame_t *f, const wb_gains_t *wb)
{
    // Open full-screen window for pixel streaming.
    ili9486_set_addr_window(0, 0, LCD_W - 1, LCD_H - 1);

    // Switch to data mode for continuous pixel streaming.
    ili9486_hal_dc(1);

    // Row buffer: 480 pixels x 2 bytes = 960 bytes.
    static uint8_t row_buf[LCD_W * 2];

    for (uint32_t oy = 0; oy < LCD_H; oy++) {
        // Source Y range (180-degree rotation: oy=0 -> bottom of sensor).
        uint32_t src_y0 = (LCD_H - 1 - oy) * FRAME_HEIGHT / LCD_H;
        uint32_t src_y1 = (LCD_H - oy) * FRAME_HEIGHT / LCD_H;
        if (src_y1 > FRAME_HEIGHT) src_y1 = FRAME_HEIGHT;

        for (uint32_t ox = 0; ox < LCD_W; ox++) {
            // Source X range (180-degree rotation: ox=0 -> right of sensor).
            uint32_t src_x0 = (LCD_W - 1 - ox) * FRAME_WIDTH / LCD_W;
            uint32_t src_x1 = (LCD_W - ox) * FRAME_WIDTH / LCD_W;
            if (src_x1 > FRAME_WIDTH) src_x1 = FRAME_WIDTH;

            // Accumulate R/G/B from Bayer mosaic (BGGR pattern).
            uint32_t r_sum = 0, g_sum = 0, b_sum = 0;
            uint32_t r_cnt = 0, g_cnt = 0, b_cnt = 0;

            for (uint32_t sy = src_y0; sy < src_y1; sy++) {
                for (uint32_t sx = src_x0; sx < src_x1; sx++) {
                    uint32_t val = unpack_raw12(f->buf, sx, sy);

                    // BGGR: even_y,even_x=B; even_y,odd_x=G;
                    //        odd_y,even_x=G;  odd_y,odd_x=R.
                    if ((sy & 1) == 0) {
                        if ((sx & 1) == 0) { b_sum += val; b_cnt++; }
                        else               { g_sum += val; g_cnt++; }
                    } else {
                        if ((sx & 1) == 0) { g_sum += val; g_cnt++; }
                        else               { r_sum += val; r_cnt++; }
                    }
                }
            }

            uint8_t r = process_channel(r_sum, r_cnt, wb->r);
            uint8_t g = process_channel(g_sum, g_cnt, wb->g);
            uint8_t b = process_channel(b_sum, b_cnt, wb->b);

            // Pack as standard RGB565: R[15:11] G[10:5] B[4:0].
            uint16_t rgb565 = ((uint16_t)(r & 0xF8) << 8) |
                              ((uint16_t)(g & 0xFC) << 3) |
                              ((uint16_t)(b) >> 3);

            // Big-endian for SPI (high byte first).
            row_buf[ox * 2]     = (uint8_t)(rgb565 >> 8);
            row_buf[ox * 2 + 1] = (uint8_t)(rgb565 & 0xFF);
        }

        // Stream this row to the LCD.
        ili9486_hal_spi_write_bulk(row_buf, LCD_W * 2);
    }
}
