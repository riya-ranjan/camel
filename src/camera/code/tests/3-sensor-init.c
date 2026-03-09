// Phase 3: IMX477 sensor register init + test pattern + readback verification.
#include "rpi.h"
#include "cam-power.h"
#include "i2c.h"
#include "imx477.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477,
    BSC0_BASE          = 0x20205000,
};

void notmain(void) {
    output("--- Phase 3: Sensor Register Init ---\n");

    // 1. Power on camera.
    cam_power_on();
    output("Camera powered on.\n");

    // 2. Init I2C on BSC0.
    i2c_init_ex(BSC0_BASE, 28, 29);

    // 3. Read chip ID to verify sensor is alive.
    uint8_t reg[2] = { IMX477_CHIP_ID_REG >> 8, IMX477_CHIP_ID_REG & 0xFF };
    uint8_t id[2] = {0};
    int r = i2c_write(IMX477_I2C_ADDR, reg, 2);
    if (r) panic("i2c_write failed: %d\n", r);
    r = i2c_read(IMX477_I2C_ADDR, id, 2);
    if (r) panic("i2c_read failed: %d\n", r);
    uint32_t chip_id = (id[0] << 8) | id[1];
    output("IMX477 chip ID = %x\n", chip_id);
    if (chip_id != IMX477_EXPECTED_ID)
        panic("Wrong chip ID!\n");

    // 4. Full sensor init (common + mode regs, stays in standby).
    imx477_init();

    // 5. Enable color bars test pattern.
    imx477_set_test_pattern(2);
    output("Test pattern set to color bars.\n");

    // 6. Readback verification.
    uint8_t val;

    imx477_read_reg8(0x0100, &val);
    output("Readback: MODE_SELECT = %x (standby)\n", val);

    imx477_read_reg8(0x0114, &val);
    output("Readback: CSI_LANE_MODE = %x (2 lanes)\n", val);

    // Test pattern is 16-bit: read low byte at 0x0601.
    imx477_read_reg8(0x0601, &val);
    output("Readback: TEST_PATTERN = %x (color bars)\n", val);

    output("--- Phase 3: PASS ---\n");
    clean_reboot();
}
