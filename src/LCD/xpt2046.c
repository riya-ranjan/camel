#include "xpt2046.h"
#include "spi.h"
#include "gpio.h"
#include "rpi.h"

static unsigned touch_cs_pin;
static unsigned touch_irq_pin;
extern spi_t lcd_spi;

void gpio_set_pullup(unsigned pin)
{
    volatile uint32_t *GPPUD      = (volatile uint32_t *)0x20200094;
    volatile uint32_t *GPPUDCLK0  = (volatile uint32_t *)0x20200098;

    *GPPUD = 2;
    delay_us(150);

    *GPPUDCLK0 = (1 << pin);
    delay_us(150);

    *GPPUD = 0;
    *GPPUDCLK0 = 0;
}

#define SPI_CLK_PIN  11
#define SPI_MOSI_PIN 10
#define SPI_MISO_PIN  9

void xpt2046_init(unsigned cs_pin, unsigned irq_pin)
{
    touch_cs_pin  = cs_pin;
    touch_irq_pin = irq_pin;

    gpio_set_output(touch_cs_pin);
    gpio_set_input(touch_irq_pin);
    gpio_set_pullup(touch_irq_pin);

    gpio_set_on(touch_cs_pin);
    delay_ms(10);
}

uint16_t xpt2046_read_raw(uint8_t cmd)
{
    uint8_t tx[3] = { cmd, 0x00, 0x00 };
    uint8_t rx[3] = { 0,   0,    0    };

    gpio_set_off(touch_cs_pin);
    delay_us(1);

    spi_n_transfer(lcd_spi, &rx[0], &tx[0], 1);
    spi_n_transfer(lcd_spi, &rx[1], &tx[1], 1);
    spi_n_transfer(lcd_spi, &rx[2], &tx[2], 1);

    gpio_set_on(touch_cs_pin);
    uint16_t raw = ((uint16_t)(rx[1] & 0x7F) << 5) |
                   ((uint16_t)(rx[2] & 0xF8) >> 3);
    return raw;
}

int xpt2046_is_touched(void)
{
    return gpio_read(touch_irq_pin) == 0;
}

int xpt2046_read(touch_point_t *tp)
{
    if (!xpt2046_is_touched()) {
        tp->touched = 0;
        return 0;
    }

    uint32_t raw_x = 0, raw_y = 0;
    const int SAMPLES = 4;
    for (int i = 0; i < SAMPLES; i++) {
        raw_x += xpt2046_read_raw(XPT2046_CMD_X);
        raw_y += xpt2046_read_raw(XPT2046_CMD_Y);
    }
    raw_x /= SAMPLES;
    raw_y /= SAMPLES;

    if (!xpt2046_is_touched()) {
        tp->touched = 0;
        return 0;
    }

    #define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
    #define MAP(v, in_lo, in_hi, out_hi) \
        (((uint32_t)CLAMP(v, in_lo, in_hi) - (in_lo)) * (out_hi) / ((in_hi) - (in_lo)))

    tp->x = MAP(raw_x, XPT2046_RAW_MIN, XPT2046_RAW_MAX, 319);
    tp->y = MAP(raw_y, XPT2046_RAW_MIN, XPT2046_RAW_MAX, 479);
    tp->touched = 1;
    return 1;
}