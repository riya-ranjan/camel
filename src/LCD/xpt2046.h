#include <stdint.h>

#define XPT2046_CMD_Y   0x90
#define XPT2046_CMD_X   0xD0

#define XPT2046_RAW_MIN  200
#define XPT2046_RAW_MAX  3900

#define XPT2046_PRESSURE_THRESHOLD  100

typedef struct {
    uint16_t x;
    uint16_t y;
    int      touched;
} touch_point_t;

void xpt2046_init(unsigned cs_pin, unsigned irq_pin);

uint16_t xpt2046_read_raw(uint8_t cmd);

int xpt2046_read(touch_point_t *tp);

int xpt2046_is_touched(void);
