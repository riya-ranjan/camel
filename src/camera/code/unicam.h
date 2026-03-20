#ifndef __UNICAM_H__
#define __UNICAM_H__

#include "rpi.h"

#define UNICAM_BASE  0x20801000

// unicam register offsets (from vc4-regs-unicam.h).
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

#define UNICAM_CLK_GATE  0x20802004

void unicam_power_init(void);

void unicam_rx_init(uint32_t bus_addr, uint32_t buf_size, uint32_t stride);

int unicam_wait_frame(uint32_t timeout_us);

void unicam_stop(void);

#endif
