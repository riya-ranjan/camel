/*
 * LCD HAL for camera build.
 * Same hardware setup as src/LCD/ili9486_hal.c but without notmain()
 * and draw_test_pattern(). Adds ili9486_hal_spi_write_bulk() for
 * streaming pixel rows.
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

#define PIN_DC    24
#define PIN_RESET 25
#define LCD_SPI_CHIP  0

static spi_t lcd_spi;

void ili9486_hal_setup(void)
{
    delay_ms(100);

    // Clock divider 40 -> ~10 MHz (ILI9486 max write = 15 MHz)
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
