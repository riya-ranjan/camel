#include "ili9486.h"
#include "xpt2046.h"
#include "rpi.h"
#include "zoom.h"

#define DISPLAY_W   ILI9486_WIDTH
#define DISPLAY_H   ILI9486_HEIGHT

static float    zoom_level  = 1.0f;
static uint16_t zoom_cx     = DISPLAY_W / 2;
static uint16_t zoom_cy     = DISPLAY_H / 2;

static uint32_t last_tap_time = 0;
#define DOUBLE_TAP_MS   400

void zoom_draw_image(const uint16_t *pixels, uint16_t img_w, uint16_t img_h)
{
    uint16_t src_w = (uint16_t)(img_w / zoom_level);
    uint16_t src_h = (uint16_t)(img_h / zoom_level);

    int img_cx = (int)((float)zoom_cx / DISPLAY_W * img_w);
    int img_cy = (int)((float)zoom_cy / DISPLAY_H * img_h);

    int src_x0 = img_cx - src_w / 2;
    int src_y0 = img_cy - src_h / 2;

    if (src_x0 < 0) src_x0 = 0;
    if (src_y0 < 0) src_y0 = 0;
    if (src_x0 + src_w > img_w) src_x0 = img_w - src_w;
    if (src_y0 + src_h > img_h) src_y0 = img_h - src_h;

    ili9486_set_addr_window(0, 0, DISPLAY_W - 1, DISPLAY_H - 1);

    ili9486_hal_dc(1);
    for (uint16_t dy = 0; dy < DISPLAY_H; dy++) {
        uint16_t sy = src_y0 + (uint16_t)((uint32_t)dy * src_h / DISPLAY_H);
        if (sy >= img_h) sy = img_h - 1;

        for (uint16_t dx = 0; dx < DISPLAY_W; dx++) {
            uint16_t sx = src_x0 + (uint16_t)((uint32_t)dx * src_w / DISPLAY_W);
            if (sx >= img_w) sx = img_w - 1;

            uint16_t pixel = pixels[sy * img_w + sx];
            uint8_t hi = pixel >> 8;
            uint8_t lo = pixel & 0xFF;
            ili9486_hal_spi_write(&hi, 1);
            ili9486_hal_spi_write(&lo, 1);
        }
    }
}

void zoom_update(const uint16_t *pixels, uint16_t img_w, uint16_t img_h)
{
    touch_point_t tp;
    if (!xpt2046_read(&tp))
        return;

    printk("touch: x=%d y=%d\n", tp.x, tp.y);


    if (!xpt2046_read(&tp))
        return;

    while (xpt2046_is_touched())
        delay_ms(10);

    uint32_t now = timer_get_usec() / 1000;
    uint32_t dt  = now - last_tap_time;

    if (dt < DOUBLE_TAP_MS) {
        zoom_level = 1.0f;
        zoom_cx    = DISPLAY_W / 2;
        zoom_cy    = DISPLAY_H / 2;
    } else {
        if (zoom_level < 4.0f) {
            zoom_level *= 2.0f;
            zoom_cx = tp.x;
            zoom_cy = tp.y;
        } else {
            zoom_level = 1.0f;
            zoom_cx    = DISPLAY_W / 2;
            zoom_cy    = DISPLAY_H / 2;
        }
    }

    last_tap_time = now;
    zoom_draw_image(pixels, img_w, img_h);
}