#ifndef __I2C_H__
#define __I2C_H__
#include "rpi.h"

// LINK TO LINUX I2C DRIVER: 
// https://android.googlesource.com/kernel/msm/+/android-7.1.0_r0.2/drivers/i2c/busses/i2c-bcm2835.c
// BCM2835 BSC register offsets (taken from Broadcom documents provided in CS140e repo, page 28)
enum {
    BSC0_BASE = 0x20205000,
    BSC_C    = BSC0_BASE + 0x00,   // Control
    BSC_S    = BSC0_BASE + 0x04,   // Status
    BSC_DLEN = BSC0_BASE + 0x08,   // Data length
    BSC_A    = BSC0_BASE + 0x0C,   // Slave address
    BSC_FIFO = BSC0_BASE + 0x10,   // Data FIFO
    BSC_DIV  = BSC0_BASE + 0x14,   // Clock divider
    BSC_CLKT = BSC0_BASE + 0x1C,   // Clock stretch timeout
};

// pg. 30 of BCM2835 datasheet
enum {
    BSC_C_I2CEN = 1 << 15,              // enables BSC controller
    BSC_C_ST    = 1 << 7,               // starts data transfer
    BSC_C_CLEAR = 1 << 4,               // clears FIFO
    BSC_C_READ  = 1 << 0,               // tells us if it is a read operation or write operation
};

// pg. 31 of BCM2835 datasheet
enum {
    BSC_S_CLKT = 1 << 9,        // detects clock timeout error 
    BSC_S_ERR  = 1 << 8,        // detects lack of acknowledgement from slave
    BSC_S_RXD  = 1 << 5,        // tells us if receiver fifo contains data
    BSC_S_TXD  = 1 << 4,        // tells us if transmitter fifo can accept more data
    BSC_S_DONE = 1 << 1,        // tells us if a transfer is complete
    BSC_S_TA   = 1 << 0,        // tells us if a transfer is in progress
}; 

// We use 28 for SDA and 29 for SCL per pg 102 of BCM2835 datasheet
enum { 
       I2C_TIMEOUT_US = 100 * 1000,
       SDA_PIN = 28,
       SCL_PIN = 29,
       I2C_CLOCK_DIV = 2500,
       I2C_CLKT_TIMEOUT = 0x1c
     };

void i2c_init();

int i2c_write(uint8_t addr, const uint8_t *data, unsigned len);

int i2c_read(uint8_t addr, uint8_t *data, unsigned len);

int i2c_write_read(uint8_t addr,
                   const uint8_t *wdata, unsigned wlen,
                   uint8_t *rdata, unsigned rlen);

#endif
