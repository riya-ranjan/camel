#ifndef __I2C_H__
#define __I2C_H__

// Bare-metal BSC I2C driver for BCM2835.
// Supports BSC0 (GPIO 0/1 or 28/29) and BSC1 (GPIO 2/3).

#include "rpi.h"

// Initialize I2C on BSC0 with specified GPIO pins (0/1 or 28/29).
void i2c_init(void);

// Initialize I2C using a specific BSC controller and GPIO pair.
//   bsc_base: 0x20205000 (BSC0) or 0x20804000 (BSC1)
//   sda_pin, scl_pin: GPIO pin numbers (set to ALT0)
void i2c_init_ex(uint32_t bsc_base, unsigned sda_pin, unsigned scl_pin);

// Write <len> bytes from <data> to I2C device at 7-bit <addr>.
// Returns 0 on success, -1 on NACK, -2 on timeout.
int i2c_write(uint8_t addr, const uint8_t *data, unsigned len);

// Read <len> bytes into <data> from I2C device at 7-bit <addr>.
// Returns 0 on success, -1 on NACK, -2 on timeout.
int i2c_read(uint8_t addr, uint8_t *data, unsigned len);

// Combined write-then-read (two separate transactions).
int i2c_write_read(uint8_t addr,
                   const uint8_t *wdata, unsigned wlen,
                   uint8_t *rdata, unsigned rlen);

#endif
