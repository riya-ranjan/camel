#ifndef __IMX477_H__
#define __IMX477_H__

#include "rpi.h"

typedef struct {
    uint16_t reg;
    uint8_t val;
} imx477_reg_t;

// Initialize sensor: common init + mode registers. Stays in standby.
void imx477_init(void);

// Start/stop streaming (MODE_SELECT register).
void imx477_start_streaming(void);
void imx477_stop_streaming(void);

// Set test pattern mode: 0=off, 2=color bars.
void imx477_set_test_pattern(uint8_t pattern);

// Single register access (exposed for readback verification).
void imx477_write_reg8(uint16_t reg, uint8_t val);
void imx477_read_reg8(uint16_t reg, uint8_t *val);

#endif
