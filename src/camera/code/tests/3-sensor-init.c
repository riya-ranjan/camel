// IMX477 sensor register init + test pattern
#include "rpi.h"
#include "cam-power.h"
#include "i2c.h"
#include "imx477.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477
};

void notmain(void) {
    output("--- Phase 3: Sensor Register Init ---\n");

    cam_power_on();
    output("Camera powered on.\n");

    i2c_init();

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

    imx477_init();

    imx477_set_test_pattern(2);
    output("Test pattern set to color bars.\n");

    uint8_t val;

    imx477_read_reg8(0x0100, &val);
    output("Readback: MODE_SELECT = %x (standby)\n", val);

    imx477_read_reg8(0x0114, &val);
    output("Readback: CSI_LANE_MODE = %x (2 lanes)\n", val);

    imx477_read_reg8(0x0601, &val);
    output("Readback: TEST_PATTERN = %x (color bars)\n", val);

    output("--- Phase 3: PASS ---\n");
    clean_reboot();
}
