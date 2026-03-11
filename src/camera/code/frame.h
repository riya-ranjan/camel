#ifndef __FRAME_H__
#define __FRAME_H__

#include "rpi.h"

// IMX477 2x2 binned mode: 2028x1520, packed RAW12.
enum {
    FRAME_WIDTH      = 2028,
    FRAME_HEIGHT     = 1520,
    FRAME_LINE_STRIDE = 3056,     // ceil(2028*1.5 / 16) * 16
    FRAME_BUF_SIZE   = 3056 * 1520,  // 4,645,120 bytes
    FRAME_ALLOC_SIZE = 5 * 1024 * 1024,
};

typedef struct {
    uint32_t gpu_handle;
    uint32_t bus_addr;
    volatile uint8_t *buf;   // ARM-accessible pointer to frame data
    int result;              // last capture result (0=ok, -1=timeout, -2=FIFO, -3=DL)
    uint32_t ibwp;           // IBWP register value after last capture
} frame_t;

// Initialize camera pipeline: boost clocks, power on camera,
// I2C + chip ID verify, unicam power domains, sensor init,
// allocate GPU DMA buffer.
void frame_init(frame_t *f);

// Capture one frame: configure unicam, start streaming, wait
// for frame, stop streaming.  Returns result code.
int frame_capture(frame_t *f);

// Initialize SD card and mount FAT32 filesystem.
// Call once before any frame_save calls.
void frame_sd_init(void);

// Save frame buffer to SD card with given 8.3 filename.
// Returns 1 on success, 0 on failure.
int frame_save(frame_t *f, const char *filename);

// Free GPU memory.
void frame_cleanup(frame_t *f);

#endif
