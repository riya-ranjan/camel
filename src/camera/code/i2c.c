#include "rpi.h"
#include "i2c.h"

// BCM2835 BSC register offsets (same layout for BSC0 and BSC1)
enum {
    BSC_C    = 0x00,   // Control
    BSC_S    = 0x04,   // Status
    BSC_DLEN = 0x08,   // Data length
    BSC_A    = 0x0C,   // Slave address
    BSC_FIFO = 0x10,   // Data FIFO
    BSC_DIV  = 0x14,   // Clock divider
    BSC_CLKT = 0x1C,   // Clock stretch timeout
};

// Control register bits
enum {
    BSC_C_I2CEN = 1 << 15,
    BSC_C_ST    = 1 << 7,
    BSC_C_CLEAR = 1 << 4,
    BSC_C_READ  = 1 << 0,
};

// Status register bits
enum {
    BSC_S_CLKT = 1 << 9,
    BSC_S_ERR  = 1 << 8,
    BSC_S_RXD  = 1 << 5,
    BSC_S_TXD  = 1 << 4,
    BSC_S_DONE = 1 << 1,
    BSC_S_TA   = 1 << 0,
};

enum { I2C_TIMEOUT_US = 100 * 1000 };

// Current BSC base address (set by init).
static uint32_t bsc_base;

static inline uint32_t bsc_reg(uint32_t off) { return bsc_base + off; }

void i2c_init_ex(uint32_t base, unsigned sda_pin, unsigned scl_pin) {
    bsc_base = base;

    gpio_set_function(sda_pin, GPIO_FUNC_ALT0);
    gpio_set_function(scl_pin, GPIO_FUNC_ALT0);
    dev_barrier();

    PUT32(bsc_reg(BSC_DIV), 2500);       // 250MHz / 2500 = 100kHz
    PUT32(bsc_reg(BSC_C), BSC_C_CLEAR);  // clear FIFO
    PUT32(bsc_reg(BSC_S), BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);
    PUT32(bsc_reg(BSC_CLKT), 0x40);
    PUT32(bsc_reg(BSC_C), BSC_C_I2CEN);

    dev_barrier();
}

void i2c_init(void) {
    // Default: BSC0 on GPIO 0/1.
    i2c_init_ex(0x20205000, 0, 1);
}

int i2c_write(uint8_t addr, const uint8_t *data, unsigned len) {
    dev_barrier();

    PUT32(bsc_reg(BSC_S), BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);
    PUT32(bsc_reg(BSC_A), addr);
    PUT32(bsc_reg(BSC_DLEN), len);

    unsigned i = 0;
    while (i < len && i < 16) {
        PUT32(bsc_reg(BSC_FIFO), data[i]);
        i++;
    }

    PUT32(bsc_reg(BSC_C), BSC_C_I2CEN | BSC_C_ST);

    uint32_t start = timer_get_usec();
    while (!(GET32(bsc_reg(BSC_S)) & BSC_S_DONE)) {
        uint32_t s = GET32(bsc_reg(BSC_S));
        if (s & BSC_S_TXD) {
            if (i < len) {
                PUT32(bsc_reg(BSC_FIFO), data[i]);
                i++;
            }
        }
        if (s & BSC_S_ERR) {
            PUT32(bsc_reg(BSC_S), BSC_S_ERR);
            dev_barrier();
            return -1;
        }
        if (s & BSC_S_CLKT) {
            PUT32(bsc_reg(BSC_S), BSC_S_CLKT);
            dev_barrier();
            return -2;
        }
        if (timer_get_usec() - start > I2C_TIMEOUT_US) {
            dev_barrier();
            return -2;
        }
    }

    uint32_t s = GET32(bsc_reg(BSC_S));
    PUT32(bsc_reg(BSC_S), BSC_S_DONE | BSC_S_ERR | BSC_S_CLKT);
    dev_barrier();

    if (s & BSC_S_ERR)  return -1;
    if (s & BSC_S_CLKT) return -2;
    return 0;
}

int i2c_read(uint8_t addr, uint8_t *data, unsigned len) {
    dev_barrier();

    PUT32(bsc_reg(BSC_S), BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);
    PUT32(bsc_reg(BSC_A), addr);
    PUT32(bsc_reg(BSC_DLEN), len);

    PUT32(bsc_reg(BSC_C), BSC_C_I2CEN | BSC_C_ST | BSC_C_READ);

    unsigned i = 0;
    uint32_t start = timer_get_usec();
    while (!(GET32(bsc_reg(BSC_S)) & BSC_S_DONE)) {
        uint32_t s = GET32(bsc_reg(BSC_S));
        if (s & BSC_S_RXD) {
            if (i < len) {
                data[i] = GET32(bsc_reg(BSC_FIFO)) & 0xFF;
                i++;
            }
        }
        if (s & BSC_S_ERR) {
            PUT32(bsc_reg(BSC_S), BSC_S_ERR);
            dev_barrier();
            return -1;
        }
        if (s & BSC_S_CLKT) {
            PUT32(bsc_reg(BSC_S), BSC_S_CLKT);
            dev_barrier();
            return -2;
        }
        if (timer_get_usec() - start > I2C_TIMEOUT_US) {
            dev_barrier();
            return -2;
        }
    }

    while (i < len && (GET32(bsc_reg(BSC_S)) & BSC_S_RXD)) {
        data[i] = GET32(bsc_reg(BSC_FIFO)) & 0xFF;
        i++;
    }

    uint32_t s = GET32(bsc_reg(BSC_S));
    PUT32(bsc_reg(BSC_S), BSC_S_DONE | BSC_S_ERR | BSC_S_CLKT);
    dev_barrier();

    if (s & BSC_S_ERR)  return -1;
    if (s & BSC_S_CLKT) return -2;
    return 0;
}

int i2c_write_read(uint8_t addr,
                   const uint8_t *wdata, unsigned wlen,
                   uint8_t *rdata, unsigned rlen) {
    int r = i2c_write(addr, wdata, wlen);
    if (r) return r;
    return i2c_read(addr, rdata, rlen);
}
