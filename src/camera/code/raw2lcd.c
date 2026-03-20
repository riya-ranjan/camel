#include "raw2lcd.h"
#include "isp.h"
#include "ili9486.h"

void raw2lcd_init(void)
{
    // landscape mode
    ili9486_set_rotation(MADCTL_MX | MADCTL_MV);
}

void raw2lcd_display(frame_t *f, const wb_gains_t *wb,
                     uint8_t *rgb_out, const wb_gains_t *jpeg_wb)
{
    // use entire screen
    ili9486_set_addr_window(0, 0, LCD_W - 1, LCD_H - 1);

    // send pixel data
    // not command, so set dc pin high
    ili9486_hal_dc(1);

    // Row buffer, 480 pixels x 2 bytes = 960 bytes
    static uint8_t row_buf[LCD_W * 2];

    for (uint32_t oy = 0; oy < LCD_H; oy++) {
        // lens flips the image 180 degrees upside down, so we need to get the opposite side pixels
        uint32_t src_y0 = (LCD_H - 1 - oy) * FRAME_HEIGHT / LCD_H;
        uint32_t src_y1 = (LCD_H - oy) * FRAME_HEIGHT / LCD_H;
        if (src_y1 > FRAME_HEIGHT) src_y1 = FRAME_HEIGHT;

        for (uint32_t ox = 0; ox < LCD_W; ox++) {
            uint32_t src_x0 = ox * FRAME_WIDTH / LCD_W;
            uint32_t src_x1 = (ox + 1) * FRAME_WIDTH / LCD_W;
            if (src_x1 > FRAME_WIDTH) src_x1 = FRAME_WIDTH;

            // accumulators used for averaged downsampling
            uint32_t r_sum = 0, g_sum = 0, b_sum = 0;
            uint32_t r_cnt = 0, g_cnt = 0, b_cnt = 0;

            for (uint32_t sy = src_y0; sy < src_y1; sy++) {
                for (uint32_t sx = src_x0; sx < src_x1; sx++) {
                    uint32_t val = unpack_raw12(f->buf, sx, sy);

                    // BGGR: even_y,even_x=B; even_y,odd_x=G;
                    //        odd_y,even_x=G;  odd_y,odd_x=R
                    if ((sy & 1) == 0) {
                        if ((sx & 1) == 0) { b_sum += val; b_cnt++; }
                        else               { g_sum += val; g_cnt++; }
                    } else {
                        if ((sx & 1) == 0) { g_sum += val; g_cnt++; }
                        else               { r_sum += val; r_cnt++; }
                    }
                }
            }

            uint8_t r = process_channel(r_sum, r_cnt, wb->r); // apply WB
            uint8_t g = process_channel(g_sum, g_cnt, wb->g);
            uint8_t b = process_channel(b_sum, b_cnt, wb->b);

            // if JPEG mode, then we need to save a separate jpeg buffer (RGB888)
            if (rgb_out && jpeg_wb) {
                uint32_t idx = (oy * LCD_W + (LCD_W - 1 - ox)) * 3;
                rgb_out[idx + 0] = process_channel(b_sum, b_cnt, jpeg_wb->r);
                rgb_out[idx + 1] = process_channel(g_sum, g_cnt, jpeg_wb->g);
                rgb_out[idx + 2] = process_channel(r_sum, r_cnt, jpeg_wb->b);
            }

            uint16_t rgb565 = ((uint16_t)(r & 0xF8) << 8) |
                              ((uint16_t)(g & 0xFC) << 3) |
                              ((uint16_t)(b) >> 3);

            // big-endian
            row_buf[ox * 2]     = (uint8_t)(rgb565 >> 8);
            row_buf[ox * 2 + 1] = (uint8_t)(rgb565 & 0xFF);
        }

        // stream row to the LCD
        ili9486_hal_spi_write_bulk(row_buf, LCD_W * 2);
    }
}
