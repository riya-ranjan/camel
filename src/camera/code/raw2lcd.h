#ifndef __RAW2LCD_H__
#define __RAW2LCD_H__

#include "rpi.h"
#include "frame.h"

// Landscape LCD dimensions (ILI9486 in 90-degree rotation)
#define LCD_W  480
#define LCD_H  320

// White balance gains in Q8.8 fixed-point (1.0 = 256).
typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} wb_gains_t;

// Calibrated WB presets from libcamera IMX477 ct_curve.
#define WB_INDOOR_2848K   ((wb_gains_t){ .r = 505, .g = 256, .b = 640 })
#define WB_DAYLIGHT_5579K ((wb_gains_t){ .r = 793, .g = 256, .b = 366 })

// Initialize LCD hardware: SPI, GPIO, display controller.
// Defined in lcd-hal.c (not in ili9486.h since it's HAL-specific).
void ili9486_hal_setup(void);

// One-time LCD setup: sets landscape mode (480x320).
// Call after ili9486_hal_setup().
void raw2lcd_init(void);

// Process RAW12 frame and stream to LCD.
// Downscales 2028x1520 -> 480x320, applies Bayer demosaic + WB + gamma.
// Rotates 180 degrees (sensor is mounted upside down).
void raw2lcd_display(frame_t *f, const wb_gains_t *wb);

// Bulk SPI write for streaming pixel rows.
// Implemented in lcd-hal.c; sends in chunks via spi_n_transfer.
void ili9486_hal_spi_write_bulk(const uint8_t *buf, uint32_t len);

#endif
