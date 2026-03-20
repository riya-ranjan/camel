#ifndef __IMX477_H__
#define __IMX477_H__

#include "rpi.h"

typedef struct {
    uint16_t reg;
    uint8_t val;
} imx477_reg_t;

void imx477_init(void);

void imx477_start_streaming(void);
void imx477_stop_streaming(void);

void imx477_set_test_pattern(uint8_t pattern);

/** we only need to write the initial mode tables, and then start and stop streaming */
void imx477_write_reg8(uint16_t reg, uint8_t val);
void imx477_read_reg8(uint16_t reg, uint8_t *val);

#endif
