#include "ili9486.h"

static void send_cmd(uint8_t cmd)
{
    ili9486_hal_dc(0);          /* D/CX low  → command */
    ili9486_hal_cs(0);          /* CSX low   → begin   */
    ili9486_hal_spi_write(&cmd, 1);
    ili9486_hal_cs(1);          /* CSX high  → end     */
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
    buf[0] = (uint8_t)(data >> 8);   /* high byte first */
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
    /*
     * CASET (2Ah) — Column Address Set
     * Parameters: SC[15:8], SC[7:0], EC[15:8], EC[7:0]
     * where SC = start column, EC = end column  (§8.2.20)
     */
    {
        uint8_t p[4] = {
            (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
            (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
        };
        write_cmd_data(ILI9486_CASET, p, 4);
    }

    /*
     * PASET (2Bh) — Page (Row) Address Set
     * Parameters: SP[15:8], SP[7:0], EP[15:8], EP[7:0]  (§8.2.21)
     */
    {
        uint8_t p[4] = {
            (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
            (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
        };
        write_cmd_data(ILI9486_PASET, p, 4);
    }
    /*
     * RAMWR (2Ch) — open the pixel data stream.
     * The caller must now send (x1-x0+1)*(y1-y0+1) RGB565 pixels.  (§8.2.22)
     */
    send_cmd(ILI9486_RAMWR);
}

void ili9486_init(void)
{
    ili9486_hal_reset(1);
    ili9486_hal_delay_ms(10);

    ili9486_hal_reset(0);          /* assert RESX low */
    ili9486_hal_delay_ms(20);      /* hold ≥ 10 µs; 20 ms is conservative */

    ili9486_hal_reset(1);          /* release reset */
    ili9486_hal_delay_ms(150);     /* wait for internal oscillator to settle */

    send_cmd(ILI9486_SWRESET);
    ili9486_hal_delay_ms(200);     /* mandatory delay after SW reset */

    send_cmd(ILI9486_SLPOUT);
    ili9486_hal_delay_ms(20);      /* wait ≥ 5 ms (§8.2.12) */

    {
        uint8_t p = COLMOD_16BPP;
        write_cmd_data(ILI9486_COLMOD, &p, 1);
    }

    {
        uint8_t p = 0x00;   // default
        // uint8_t p = 0x08;   // BGR only
        // uint8_t p = 0x40;   // MX only  
        // uint8_t p = 0x48;   // MX + BGR
        // uint8_t p = 0x80;   // MY only
        // uint8_t p = 0x88;   // MY + BGR
        // uint8_t p = 0xC8;   // MY + MX + BGR
        // uint8_t p = 0x68;   // MX + MV + BGR  (landscape)
        write_cmd_data(ILI9486_MADCTL, &p, 1);
    }

    /* Power Control 1 (C0h): AVDD=6.8V, aVCL=-3.4V, VGH=15V, VGL=-10V */
    {
        uint8_t p[2] = { 0x0D, 0x0D };
        write_cmd_data(ILI9486_PWCTR1, p, 2);
    }

    /* Power Control 2 (C1h): step-up factor for VGH/VGL circuits */
    {
        uint8_t p = 0x41;
        write_cmd_data(ILI9486_PWCTR2, &p, 1);
    }

    /* Power Control 3 (C2h): normal mode op-amp current */
    {
        uint8_t p = 0x44;
        write_cmd_data(ILI9486_PWCTR3, &p, 1);
    }

    /* VCOM Control (C5h): VCOMH / VCOML levels */
    {
        uint8_t p[4] = { 0x00, 0x00, 0x00, 0x00 };
        write_cmd_data(ILI9486_VMCTR, p, 4);
    }

    /* ---- 8. Normal display mode ------------------------------------- */
    send_cmd(ILI9486_NORON);
    ili9486_hal_delay_ms(10);

    /* ---- 9. Display ON ---------------------------------------------- */
    ili9486_hal_delay_ms(20);
    send_cmd(ILI9486_DISPON);
    ili9486_hal_delay_ms(20);
}

void ili9486_set_rotation(uint8_t rotation)
{
    uint8_t madctl = rotation & (MADCTL_MY | MADCTL_MX | MADCTL_MV |
                                  MADCTL_ML | MADCTL_BGR | MADCTL_MH);
    write_cmd_data(ILI9486_MADCTL, &madctl, 1);
}

void ili9486_fill_rect(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       uint16_t color)
{
    if (w == 0 || h == 0)
        return;
    if (x >= ILI9486_WIDTH || y >= ILI9486_HEIGHT)
        return;

    /* Clamp to display boundaries */
    if ((uint32_t)x + w > ILI9486_WIDTH)
        w = ILI9486_WIDTH - x;
    if ((uint32_t)y + h > ILI9486_HEIGHT)
        h = ILI9486_HEIGHT - y;

    ili9486_set_addr_window(x, y, x + w - 1, y + h - 1);
    // ili9486_set_addr_window(0, 0, 319, 479);

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
    ili9486_fill_rect(0, 0, ILI9486_WIDTH, ILI9486_HEIGHT, color);
}

void ili9486_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ILI9486_WIDTH || y >= ILI9486_HEIGHT)
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
    if (x >= ILI9486_WIDTH || y >= ILI9486_HEIGHT)
        return;

    /* Clamp */
    if ((uint32_t)x + w > ILI9486_WIDTH)
        w = ILI9486_WIDTH - x;
    if ((uint32_t)y + h > ILI9486_HEIGHT)
        h = ILI9486_HEIGHT - y;

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