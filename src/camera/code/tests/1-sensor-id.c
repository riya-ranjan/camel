// Phase 1: Power on camera, read IMX477 chip ID over I2C.
#include "rpi.h"
#include "cam-power.h"
#include "i2c.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477,
    BSC0_BASE          = 0x20205000,
};

void notmain(void) {
    output("--- Phase 1: Camera Power & I2C ---\n");

    // 1. Power on camera (GPIO 44: LOW→HIGH with 500ms settle).
    cam_power_on();
    output("Camera powered on.\n");

    // 2. Init I2C on BSC0, GPIO 28 (SDA) / 29 (SCL), 100kHz.
    i2c_init_ex(BSC0_BASE, 28, 29);
    output("I2C initialized.\n");

    // 3. Read chip ID: write 16-bit register address, read 2 bytes.
    uint8_t reg[2] = { IMX477_CHIP_ID_REG >> 8, IMX477_CHIP_ID_REG & 0xFF };
    uint8_t id[2] = {0};
    int r = i2c_write(IMX477_I2C_ADDR, reg, 2);
    if (r) panic("i2c_write failed: %d\n", r);
    r = i2c_read(IMX477_I2C_ADDR, id, 2);
    if (r) panic("i2c_read failed: %d\n", r);

    uint32_t chip_id = (id[0] << 8) | id[1];
    output("IMX477 chip ID = 0x%x (expected 0x%x)\n", chip_id, IMX477_EXPECTED_ID);

    if (chip_id == IMX477_EXPECTED_ID)
        output("--- Phase 1: PASS ---\n");
    else
        panic("Wrong chip ID!\n");

    clean_reboot();
}
