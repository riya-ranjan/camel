/*
 *   -----------   --------   ------
 *   SCL           GPIO 11    SPI0 CLK  (hardware SPI, fixed)
 *   SDA           GPIO 10    SPI0 MOSI (hardware SPI, fixed)
 *   CSX           GPIO  8    SPI0 CE0 — asserted automatically by
 *                                       spi_n_transfer() for spi_chip=0
 *   D/CX          GPIO 24    Plain GPIO output: LOW=command, HIGH=data
 *   RESX          GPIO 25    Plain GPIO output: LOW=in reset, HIGH=running
 *   VCC           3.3 V
 *   GND           GND
 */

#include "ili9486.h"
#include "spi.h"       /* spi_t, spi_n_init(), spi_n_transfer() */
#include "gpio.h"      /* gpio_set_output(), gpio_set_on(), gpio_set_off() */
#include "rpi.h"     /* delay_ms() */

/* GPIO pin numbers — change to match your wiring */
#define PIN_DC    24
#define PIN_RESET 25

/* SPI chip-select index: 0 = CE0 (GPIO 8), 1 = CE1 (GPIO 7) */
#define LCD_SPI_CHIP  0

/* Module-private SPI handle, set by ili9486_hal_setup() */
static spi_t lcd_spi;

void ili9486_hal_setup(void)
{
    /* Power-on settling time, same reasoning as nrf_spi_init() */
    delay_ms(100);

    /*
     * Clock divider 40 -> ~10 MHz on a 400 MHz RPi core clock.
     * The ILI9486 serial write max is 15 MHz, so this is safely below.
     * (Same divider the NRF driver uses.)
     */
    lcd_spi = spi_n_init(LCD_SPI_CHIP, 40);
    delay_ms(100);

    dev_barrier();

    /* D/CX and RESX are plain output GPIOs, not part of the SPI bus */
    gpio_set_output(PIN_DC);
    gpio_set_output(PIN_RESET);

    /* Safe idle state: not in reset, DC=data */
    gpio_set_on(PIN_RESET);
    gpio_set_on(PIN_DC);

    dev_barrier();
    delay_ms(10);

    /* Now bring up the display controller */
    ili9486_init();
}

/* CS is toggled automatically by spi_n_transfer() -- nothing to do. */
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
    delay_ms(ms);   /* same function used throughout cs140e */
}

void ili9486_hal_spi_write(const uint8_t *buf, uint32_t len)
{
    uint8_t rx[64];
    assert(len <= sizeof rx);
    spi_n_transfer(lcd_spi, rx, (uint8_t *)buf, len);
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

void notmain(void)
{
    /* ili9486_hal_setup() calls ili9486_init() internally */
    ili9486_hal_setup();

    // ili9486_fill_screen(ILI9486_CYAN);
    // ili9486_fill_rect(10, 10, 100, 80, ILI9486_RED);
    // ili9486_draw_pixel(ILI9486_WIDTH / 2, ILI9486_HEIGHT / 2, ILI9486_RED);
    ili9486_fill_screen(ILI9486_BLACK);          // clear
    ili9486_draw_pixel(0,   0,   ILI9486_RED);   // should be top-left
    ili9486_draw_pixel(319, 479, ILI9486_RED); // should be bottom-right
    draw_test_pattern();
    while (1) {}
}


// todo
// image display
// double buffer
// video/multiframe display