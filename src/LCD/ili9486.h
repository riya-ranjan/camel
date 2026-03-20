#ifndef ILI9486_H
#define ILI9486_H

#include <stdint.h>

#define ILI9486_WIDTH   320
#define ILI9486_HEIGHT  480

// commands
#define ILI9486_NOP        0x00  /* No Operation */
#define ILI9486_SWRESET    0x01  /* Software Reset */
#define ILI9486_SLPIN      0x10  /* Sleep IN */
#define ILI9486_SLPOUT     0x11  /* Sleep OUT */
#define ILI9486_PTLON      0x12  /* Partial Mode ON */
#define ILI9486_NORON      0x13  /* Normal Display Mode ON */
#define ILI9486_INVOFF     0x20  /* Display Inversion OFF */
#define ILI9486_INVON      0x21  /* Display Inversion ON */
#define ILI9486_DISPOFF    0x28  /* Display OFF */
#define ILI9486_DISPON     0x29  /* Display ON */
#define ILI9486_CASET      0x2A  /* Column Address Set */
#define ILI9486_PASET      0x2B  /* Page (Row) Address Set */
#define ILI9486_RAMWR      0x2C  /* Memory Write */
#define ILI9486_RAMRD      0x2E  /* Memory Read */
#define ILI9486_PLTAR      0x30  /* Partial Area */
#define ILI9486_VSCRDEF    0x33  /* Vertical Scrolling Definition */
#define ILI9486_TEOFF      0x34  /* Tearing Effect Line OFF */
#define ILI9486_TEON       0x35  /* Tearing Effect Line ON */
#define ILI9486_MADCTL     0x36  /* Memory Access Control */
#define ILI9486_VSCRSADD   0x37  /* Vertical Scrolling Start Address */
#define ILI9486_IDMOFF     0x38  /* Idle Mode OFF */
#define ILI9486_IDMON      0x39  /* Idle Mode ON */
#define ILI9486_COLMOD     0x3A  /* Interface Pixel Format */
#define ILI9486_RAMWRC     0x3C  /* Memory Write Continue */

// Extended / power control commands
#define ILI9486_PWCTR1     0xC0  /* Power Control 1 */
#define ILI9486_PWCTR2     0xC1  /* Power Control 2 */
#define ILI9486_PWCTR3     0xC2  /* Power Control 3 */
#define ILI9486_PWCTR4     0xC3  /* Power Control 4 */
#define ILI9486_PWCTR5     0xC4  /* Power Control 5 */
#define ILI9486_VMCTR      0xC5  /* VCOM Control */
#define ILI9486_PGAMCTRL   0xE0  /* Positive Gamma Control */
#define ILI9486_NGAMCTRL   0xE1  /* Negative Gamma Control */

#define MADCTL_MY   0x80  /* Row address order */
#define MADCTL_MX   0x40  /* Column address order */
#define MADCTL_MV   0x20  /* Row/column exchange */
#define MADCTL_ML   0x10  /* Vertical refresh */
#define MADCTL_BGR  0x08  /* Color order */
#define MADCTL_MH   0x04  /* Horizontal refresh */

#define ILI9486_ROTATION_0    (0)
#define ILI9486_ROTATION_90   (MADCTL_MX | MADCTL_MV)
#define ILI9486_ROTATION_180  (MADCTL_MX | MADCTL_MY)
#define ILI9486_ROTATION_270  (MADCTL_MY | MADCTL_MV)

#define COLMOD_16BPP  0x55  /* DPI=101 */
#define COLMOD_18BPP  0x66  /* DPI=110 */

#define RGB565(r, g, b) \
    (uint16_t)(((uint16_t)(b) & 0xF8) << 8 | \
               ((uint16_t)(g) & 0xFC) << 3 | \
               ((uint16_t)(r) & 0xF8) >> 3)


#define ILI9486_BLACK   0x0000
#define ILI9486_WHITE   0xFFFF
#define ILI9486_RED     RGB565(255, 0, 0)
#define ILI9486_GREEN   RGB565(0, 255, 0)
#define ILI9486_BLUE    RGB565(0, 0, 255)
#define ILI9486_CYAN    RGB565(0, 255, 255)
#define ILI9486_MAGENTA RGB565(255, 0, 255)
#define ILI9486_YELLOW  RGB565(255, 255, 0)

void ili9486_init(void);

void ili9486_set_rotation(uint8_t rotation);

void ili9486_set_addr_window(uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1);


void ili9486_fill_rect(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       uint16_t color);


void ili9486_fill_screen(uint16_t color);

void ili9486_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

void ili9486_draw_image(uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h,
                        const uint16_t *pixels);

void ili9486_hal_cs(int level);

void ili9486_hal_dc(int level);

void ili9486_hal_reset(int level);

void ili9486_hal_delay_ms(uint32_t ms);

void ili9486_hal_spi_write(const uint8_t *buf, uint32_t len);
void ili9486_hal_spi_write_bulk(const uint8_t *buf, uint32_t len);

#endif /* ILI9486_H */