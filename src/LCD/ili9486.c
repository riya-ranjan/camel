#include "ili9486.h"

static uint16_t cur_w = ILI9486_WIDTH;
static uint16_t cur_h = ILI9486_HEIGHT;

static void send_cmd(uint8_t cmd)
{
    ili9486_hal_dc(0);
    ili9486_hal_cs(0);
    ili9486_hal_spi_write(&cmd, 1);
    ili9486_hal_cs(1);
}

static void send_data8(uint8_t data)
{
    ili9486_hal_dc(1);
    ili9486_hal_cs(0);
    ili9486_hal_spi_write(&data, 1);
    ili9486_hal_cs(1);
}

static void send_data16(uint16_t data)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(data >> 8);
    buf[1] = (uint8_t)(data & 0xFF);
    ili9486_hal_dc(1);
    ili9486_hal_cs(0);
    ili9486_hal_spi_write(buf, 2);
    ili9486_hal_cs(1);
}

static void write_cmd_data(uint8_t cmd, const uint8_t *data, uint32_t len)
{
    ili9486_hal_dc(0);
    ili9486_hal_spi_write(&cmd, 1);

    if (data && len) {
        ili9486_hal_dc(1);
        for (uint32_t i = 0; i < len; i++)
            ili9486_hal_spi_write(&data[i], 1);
    }
}

void ili9486_set_addr_window(uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1)
{
    {
        uint8_t p[4] = {
            (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
            (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
        };
        write_cmd_data(ILI9486_CASET, p, 4);
    }

    {
        uint8_t p[4] = {
            (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
            (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
        };
        write_cmd_data(ILI9486_PASET, p, 4);
    }
    send_cmd(ILI9486_RAMWR);
}

void ili9486_init(void)
{
    ili9486_hal_reset(1);
    ili9486_hal_delay_ms(10);

    ili9486_hal_reset(0);  
    ili9486_hal_delay_ms(20);

    ili9486_hal_reset(1);
    ili9486_hal_delay_ms(150);

    send_cmd(ILI9486_SWRESET);
    ili9486_hal_delay_ms(200);    // mandatory delay after SW reset

    send_cmd(ILI9486_SLPOUT);
    ili9486_hal_delay_ms(20);

    {
        uint8_t p = COLMOD_16BPP;
        write_cmd_data(ILI9486_COLMOD, &p, 1);
    }

    {
        uint8_t p = 0x00;   // default
        write_cmd_data(ILI9486_MADCTL, &p, 1);
    }

    {
        uint8_t p[2] = { 0x0D, 0x0D };
        write_cmd_data(ILI9486_PWCTR1, p, 2);
    }

    {
        uint8_t p = 0x41;
        write_cmd_data(ILI9486_PWCTR2, &p, 1);
    }

    {
        uint8_t p = 0x44;
        write_cmd_data(ILI9486_PWCTR3, &p, 1);
    }

    {
        uint8_t p[4] = { 0x00, 0x00, 0x00, 0x00 };
        write_cmd_data(ILI9486_VMCTR, p, 4);
    }

    // normal display mode
    send_cmd(ILI9486_NORON);
    ili9486_hal_delay_ms(10);

    // display on
    ili9486_hal_delay_ms(20);
    send_cmd(ILI9486_DISPON);
    ili9486_hal_delay_ms(20);
}

void ili9486_set_rotation(uint8_t rotation)
{
    uint8_t madctl = rotation & (MADCTL_MY | MADCTL_MX | MADCTL_MV |
                                  MADCTL_ML | MADCTL_BGR | MADCTL_MH);
    write_cmd_data(ILI9486_MADCTL, &madctl, 1);

    if (madctl & MADCTL_MV) {
        cur_w = ILI9486_HEIGHT;  // 480
        cur_h = ILI9486_WIDTH;   // 320
    } else {
        cur_w = ILI9486_WIDTH;   // 320
        cur_h = ILI9486_HEIGHT;  // 480
    }
}

void ili9486_fill_rect(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       uint16_t color)
{
    if (w == 0 || h == 0)
        return;
    if (x >= cur_w || y >= cur_h)
        return;

    // clamp to display
    if ((uint32_t)x + w > cur_w)
        w = cur_w - x;
    if ((uint32_t)y + h > cur_h)
        h = cur_h - y;

    ili9486_set_addr_window(x, y, x + w - 1, y + h - 1);

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t buf[2] = { hi, lo };

    uint32_t total = (uint32_t)w * h;

    ili9486_hal_dc(1);
    ili9486_hal_cs(0);
    for (uint32_t i = 0; i < total; i++) {
        ili9486_hal_spi_write(buf, 2);
    }
    ili9486_hal_cs(1);
}

void ili9486_fill_screen(uint16_t color)
{
    ili9486_fill_rect(0, 0, cur_w, cur_h, color);
}

void ili9486_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= cur_w || y >= cur_h)
        return;

    ili9486_set_addr_window(x, y, x, y);

    uint8_t buf[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    ili9486_hal_dc(1);
    ili9486_hal_cs(0);
    ili9486_hal_spi_write(buf, 2);
    ili9486_hal_cs(1);
}

void ili9486_draw_image(uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h,
                        const uint16_t *pixels)
{
    if (!pixels || w == 0 || h == 0)
        return;
    if (x >= cur_w || y >= cur_h)
        return;

    // clamp
    if ((uint32_t)x + w > cur_w)
        w = cur_w - x;
    if ((uint32_t)y + h > cur_h)
        h = cur_h - y;

    ili9486_set_addr_window(x, y, x + w - 1, y + h - 1);

    uint32_t total = (uint32_t)w * h;

    ili9486_hal_dc(1);
    ili9486_hal_cs(0);
    for (uint32_t i = 0; i < total; i++) {
        uint8_t buf[2] = {
            (uint8_t)(pixels[i] >> 8),
            (uint8_t)(pixels[i] & 0xFF)
        };
        ili9486_hal_spi_write(buf, 2);
    }
    ili9486_hal_cs(1);
}