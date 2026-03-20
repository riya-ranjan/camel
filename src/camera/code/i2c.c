#include "rpi.h"
#include "i2c.h"

void i2c_init() {
    gpio_set_function(SDA_PIN, GPIO_FUNC_ALT0);
    gpio_set_function(SCL_PIN, GPIO_FUNC_ALT0);
    dev_barrier();

    PUT32(BSC_DIV, I2C_CLOCK_DIV);       // 250MHz / 2500 = 100kHz
    PUT32(BSC_C, BSC_C_CLEAR);  // clear FIFO
    PUT32(BSC_S, BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);   // clear clock timeout, ack errors, and done bit from status reg
    PUT32(BSC_CLKT, I2C_CLKT_TIMEOUT);
    PUT32(BSC_C, BSC_C_I2CEN);  // enable once done with settings

    dev_barrier();
}

int i2c_write(uint8_t addr, const uint8_t *data, unsigned len) {
    dev_barrier();

    PUT32(BSC_S, BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);
    PUT32(BSC_A, addr);         // set slave address (pg. 33 of BCM2835 datasheet)
    PUT32(BSC_DLEN, len);       // set amount of data that needs to be sent (pg. 33 of BCM2835 datasheet)

    unsigned i = 0;
    while (i < len && i < 16) {         // FIFO can store 16 bytes at a time
        PUT32(BSC_FIFO, data[i]);
        i++;
    }

    PUT32(BSC_C, BSC_C_I2CEN | BSC_C_ST);       // enable I2C & start a new transfer (pg. 30 of BCM2835 datasheet)

    uint32_t start = timer_get_usec();
    while (!(GET32(BSC_S) & BSC_S_DONE)) {  // wait until transfer is complete
        uint32_t s = GET32(BSC_S);
        if (s & BSC_S_TXD) {            // check if transmitter FIFO can accept more data
            if (i < len) {
                PUT32(BSC_FIFO, data[i]);       
                i++;
            }
        }
        /** CHECK FOR ERRORS AND TIMEOUT */
        if (s & BSC_S_ERR) {
            PUT32(BSC_S, BSC_S_ERR);
            dev_barrier();
            return -1;
        }
        if (s & BSC_S_CLKT) {
            PUT32(BSC_S, BSC_S_CLKT);
            dev_barrier();
            return -2;
        }
        if (timer_get_usec() - start > I2C_TIMEOUT_US) {
            dev_barrier();
            return -2;
        }
    }

    uint32_t s = GET32(BSC_S);
    PUT32(BSC_S, BSC_S_DONE | BSC_S_ERR | BSC_S_CLKT);      // clear any errors 
    dev_barrier();

    if (s & BSC_S_ERR)  return -1;
    if (s & BSC_S_CLKT) return -2;
    return 0;
}

int i2c_read(uint8_t addr, uint8_t *data, unsigned len) {
    dev_barrier();

    PUT32(BSC_S, BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);
    PUT32(BSC_A, addr);
    PUT32(BSC_DLEN, len);

    PUT32(BSC_C, BSC_C_I2CEN | BSC_C_ST | BSC_C_READ);      // enable I2C & start a new transfer, set to read (pg. 30 of BCM2835 datasheet)

    unsigned i = 0;
    uint32_t start = timer_get_usec();
    while (!(GET32(BSC_S) & BSC_S_DONE)) {
        uint32_t s = GET32(BSC_S);
        if (s & BSC_S_RXD) {            // check if receiver FIFO contains data
            if (i < len) {
                data[i] = GET32(BSC_FIFO) & 0xFF; // get data from FIFO (bottom 8 bits are data), pg. 33 of BCM2835 datasheet
                i++;
            }
        }
        if (s & BSC_S_ERR) {
            PUT32(BSC_S, BSC_S_ERR);
            dev_barrier();
            return -1;
        }
        if (s & BSC_S_CLKT) {
            PUT32(BSC_S, BSC_S_CLKT);
            dev_barrier();
            return -2;
        }
        if (timer_get_usec() - start > I2C_TIMEOUT_US) {
            dev_barrier();
            return -2;
        }
    }

    while (i < len && (GET32(BSC_S) & BSC_S_RXD)) { 
        data[i] = GET32(BSC_FIFO) & 0xFF;
        i++;
    }

    uint32_t s = GET32(BSC_S);
    PUT32(BSC_S, BSC_S_DONE | BSC_S_ERR | BSC_S_CLKT);
    dev_barrier();

    if (s & BSC_S_ERR)  return -1;
    if (s & BSC_S_CLKT) return -2;
    return 0;
}

int i2c_write_read(uint8_t addr,
                   const uint8_t *wdata, unsigned wlen,
                   uint8_t *rdata, unsigned rlen) {
    int r = i2c_write(addr, wdata, wlen);       // tell the slave which register we want
    if (r) return r;
    return i2c_read(addr, rdata, rlen);         // read back the register's data
}
