#ifndef __RAW2LCD_H__
#define __RAW2LCD_H__

#include "rpi.h"
#include "frame.h"

#define LCD_W  480
#define LCD_H  320

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} wb_gains_t;

#define WB_INDOOR_2848K   ((wb_gains_t){ .r = 505, .g = 256, .b = 640 })

// JPEG WB presets
#define JPEG_WB_INDOOR    ((wb_gains_t){ .r = 540, .g = 256, .b = 630 })

// initialize LCD hardware
void ili9486_hal_setup(void);

// sets landscape mode (480x320)
void raw2lcd_init(void);

// process RAW12 frame and stream to LCD
void raw2lcd_display(frame_t *f, const wb_gains_t *wb,
                     uint8_t *rgb_out, const wb_gains_t *jpeg_wb);

// bulk SPI write for streaming pixel rows
// implemented in lcd-hal.c; sends in chunks via spi_n_transfer
void ili9486_hal_spi_write_bulk(const uint8_t *buf, uint32_t len);

#endif
