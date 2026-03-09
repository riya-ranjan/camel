#ifndef __UNICAM_H__
#define __UNICAM_H__

#include "rpi.h"

// BCM2835 Unicam CSI-2 receiver (CAM1) base address.
#define UNICAM_BASE  0x20801000

// Unicam register offsets (from vc4-regs-unicam.h).
enum {
    UNICAM_CTRL  = 0x000,
    UNICAM_STA   = 0x004,
    UNICAM_ANA   = 0x008,
    UNICAM_PRI   = 0x00C,
    UNICAM_CLK   = 0x010,
    UNICAM_CLT   = 0x014,
    UNICAM_DAT0  = 0x018,
    UNICAM_DAT1  = 0x01C,
    UNICAM_DLT   = 0x028,
    UNICAM_CMP0  = 0x02C,
    UNICAM_CAP0  = 0x034,
    UNICAM_ICTL  = 0x100,
    UNICAM_ISTA  = 0x104,
    UNICAM_IDI0  = 0x108,
    UNICAM_IPIPE = 0x10C,
    UNICAM_IBSA0 = 0x110,
    UNICAM_IBEA0 = 0x114,
    UNICAM_IBLS  = 0x118,
    UNICAM_IBWP  = 0x11C,
    UNICAM_IHWIN = 0x120,
    UNICAM_IVWIN = 0x128,
    UNICAM_ICC   = 0x130,
    UNICAM_DCS   = 0x200,
    UNICAM_DBSA0 = 0x204,
    UNICAM_DBEA0 = 0x208,
    UNICAM_MISC  = 0x400,
};

// Clock gate register (outside Unicam block).
#define UNICAM_CLK_GATE  0x20802004

// Enable power domains and CAM1 clock for Unicam.
void unicam_power_init(void);

// Configure the Unicam CSI-2 receiver for capture.
//   bus_addr: GPU bus address of DMA buffer
//   buf_size: buffer size in bytes
//   stride:   line stride in bytes (must be 16-byte aligned)
void unicam_rx_init(uint32_t bus_addr, uint32_t buf_size, uint32_t stride);

// Wait for a frame-end interrupt. Returns 0 on success, -1 timeout, -2 error.
int unicam_wait_frame(uint32_t timeout_us);

// Stop the Unicam receiver.
void unicam_stop(void);

#endif
