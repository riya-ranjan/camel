/*
 * LCD HAL for camera build.
 *
 * SCL/CLK  - GPIO 11 (SPI0 CLK)
 * SDA/MOSI - GPIO 10 (SPI0 MOSI)
 * CSX - GPIO 8 (SPI0 CE0)
 * D/CX - GPIO 24
 * RESX  - GPIO 25
 */

#include "ili9486.h"
#include "spi.h"
#include "gpio.h"
#include "rpi.h"
#include "xpt2046.h"
#include "zoom.h"

#define PIN_DC    24
#define PIN_RESET 25
#define LCD_SPI_CHIP  0

static spi_t lcd_spi;

void ili9486_hal_setup(void)
{
    delay_ms(100);

    // Clock divider 40 -> ~10 MHz (ILI9486 max write = 15 MHz).
    lcd_spi = spi_n_init(LCD_SPI_CHIP, 40);
    delay_ms(100);

    dev_barrier();

    gpio_set_output(PIN_DC);
    gpio_set_output(PIN_RESET);

    gpio_set_on(PIN_RESET);
    gpio_set_on(PIN_DC);

    dev_barrier();
    delay_ms(10);

    ili9486_init();
}

void ili9486_hal_cs(int level) { (void)level; }

void ili9486_hal_dc(int level)
{
    if (level) gpio_set_on(PIN_DC);
    else       gpio_set_off(PIN_DC);
}

void ili9486_hal_reset(int level)
{
    if (level) gpio_set_on(PIN_RESET);
    else       gpio_set_off(PIN_RESET);
}

void ili9486_hal_delay_ms(uint32_t ms)
{
    delay_ms(ms);
}

void ili9486_hal_spi_write(const uint8_t *buf, uint32_t len)
{
    uint8_t rx[64];
    assert(len <= sizeof rx);
    spi_n_transfer(lcd_spi, rx, (uint8_t *)buf, len);
}

// Bulk SPI write: sends data in 480-byte chunks for streaming
// pixel rows without the 64-byte limit of ili9486_hal_spi_write
void ili9486_hal_spi_write_bulk(const uint8_t *buf, uint32_t len)
{
    static uint8_t rx[480];
    while (len > 0) {
        uint32_t n = (len < sizeof(rx)) ? len : sizeof(rx);
        spi_n_transfer(lcd_spi, rx, (uint8_t *)buf, n);
        buf += n;
        len -= n;
    }
}

void draw_test_pattern(void);
void draw_test_pattern(void)
{
    for (uint16_t y = 0; y < ILI9486_HEIGHT; y++) {
        for (uint16_t x = 0; x < ILI9486_WIDTH; x++) {
            uint16_t color;
            uint16_t stripe = x / 40;  // 8 stripes of 40px each
            switch (stripe) {
                case 0: color = 0x0000; break; // black
                case 1: color = 0xF800; break; // red
                case 2: color = 0x07E0; break; // green
                case 3: color = 0x001F; break; // blue
                case 4: color = 0x07FF; break; // cyan
                case 5: color = 0xF81F; break; // magenta
                case 6: color = 0xFFE0; break; // yellow
                case 7: color = 0xFFFF; break; // white
                default: color = 0x0000; break;
            }
            ili9486_draw_pixel(x, y, color);
        }
    }
}

#define TOUCH_CS_PIN   16 
#define TOUCH_IRQ_PIN  17 

// 4x4 grid of solid color blocks to make zoom easy to verify
void draw_zoom_test(void)
{
    uint16_t colors[4][4] = {
        { 0xF800, 0x07E0, 0x001F, 0xFFFF },  // red, green, blue, white
        { 0xFFE0, 0x07FF, 0xF81F, 0x0000 },  // yellow, cyan, magenta, black
        { 0xF800, 0x07E0, 0x001F, 0xFFFF },  // repeat
        { 0xFFE0, 0x07FF, 0xF81F, 0x0000 },
    };

    uint16_t block_w = ILI9486_WIDTH  / 4;   // 80px
    uint16_t block_h = ILI9486_HEIGHT / 4;   // 120px

    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            ili9486_fill_rect(col * block_w, row * block_h,
                              block_w, block_h,
                              colors[row][col]);
}

void notmain(void)
{
    ili9486_hal_setup();
    xpt2046_init(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

    // ili9486_fill_screen(ILI9486_CYAN);
    // ili9486_fill_rect(10, 10, 100, 80, ILI9486_RED);
    // ili9486_draw_pixel(ILI9486_WIDTH / 2, ILI9486_HEIGHT / 2, ILI9486_RED);
    ili9486_fill_screen(ILI9486_BLACK);          // clear
    // ili9486_draw_pixel(0,   0,   ILI9486_RED);   // should be top-left
    // ili9486_draw_pixel(319, 479, ILI9486_RED); // should be bottom-right
    // draw_test_pattern();
    draw_zoom_test();

    static uint16_t test_pixels[ILI9486_WIDTH * ILI9486_HEIGHT];

    uint16_t colors[4][4] = {
        { 0xF800, 0x07E0, 0x001F, 0xFFFF },
        { 0xFFE0, 0x07FF, 0xF81F, 0x0000 },
        { 0xF800, 0x07E0, 0x001F, 0xFFFF },
        { 0xFFE0, 0x07FF, 0xF81F, 0x0000 },
    };

    uint16_t block_w = ILI9486_WIDTH  / 4;
    uint16_t block_h = ILI9486_HEIGHT / 4;

    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            for (uint16_t y = row * block_h; y < (row+1) * block_h; y++)
                for (uint16_t x = col * block_w; x < (col+1) * block_w; x++)
                    test_pixels[y * ILI9486_WIDTH + x] = colors[row][col];

    while (1) {
        zoom_update(test_pixels, ILI9486_WIDTH, ILI9486_HEIGHT);
        delay_ms(50);
    }
}
