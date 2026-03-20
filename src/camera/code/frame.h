#ifndef __FRAME_H__
#define __FRAME_H__

#include "rpi.h"

enum {
    FRAME_WIDTH      = 2028,        // IMX477 gives 2028x1520 output, datasheet pg.23
    FRAME_HEIGHT     = 1520,
    FRAME_LINE_STRIDE = 3056,     // need 1.5 bytes per pixel
    FRAME_BUF_SIZE   = 3056 * 1520,  
    FRAME_ALLOC_SIZE = 5 * 1024 * 1024, // ~5 MB needed per frame

    JPEG_WIDTH       = 480,         // smaller image to match LCD
    JPEG_HEIGHT      = 320,
    JPEG_RGB_SIZE    = 480 * 320 * 3,             // 3 bytes since jpeg uses RGB 888
    JPEG_OUT_SIZE    = 512 * 1024,                
};

typedef struct {
    uint32_t gpu_handle;
    uint32_t bus_addr;
    volatile uint8_t *buf;   
    int result;             
    uint32_t ibwp;           
} frame_t;

void frame_init(frame_t *f);

int frame_capture(frame_t *f);

void frame_sd_init(void);

// save .RAW file to SD card (used for actual photographers?)
int frame_save(frame_t *f, const char *filename);

// save pre-computed RGB888 image as JPEG to SD card
int frame_save_jpeg_rgb(const uint8_t *rgb, int width, int height,
                        const char *filename, int quality);

void frame_cleanup(frame_t *f);

#endif
