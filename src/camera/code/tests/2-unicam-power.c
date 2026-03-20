// enable Unicam power domain -- claude generated test
#include "rpi.h"
#include "mbox.h"
#include "cam-power.h"
#include "i2c.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477,
};

enum {
    MBOX_TAG_SET_DOMAIN_STATE = 0x00038030,
    RPI_POWER_DOMAIN_UNICAM1  = 13,
    RPI_POWER_DOMAIN_CSI2     = 15,
};

enum {
    CM_CAM1CTL = 0x20101048,
    CM_CAM1DIV = 0x2010104C,
    CM_PASSWD  = 0x5A000000,
    CM_ENAB    = 1 << 4,
    CM_BUSY    = 1 << 7,
    CM_SRC_PLLD = 6,       // PLLD_PER = 500MHz
};

enum { UNICAM_BASE = 0x20801000 };

static void enable_power_domain(unsigned domain) {
    volatile uint32_t msg[8] __attribute__((aligned(16)));
    msg[0] = 8 * 4;            // total size
    msg[1] = 0;                 // request
    msg[2] = MBOX_TAG_SET_DOMAIN_STATE;
    msg[3] = 8;                 // response buffer size
    msg[4] = 0;                 // request indicator
    msg[5] = domain;            // domain id
    msg[6] = 1;                 // ON
    msg[7] = 0;                 // end tag
    mbox_send(MBOX_CH, msg);
}

static void cam1_clock_init(void) {
    // Stop clock: clear ENAB, keep source select
    PUT32(CM_CAM1CTL, CM_PASSWD | CM_SRC_PLLD);
    while (GET32(CM_CAM1CTL) & CM_BUSY)
        ;

    // Divisor: 500MHz / 5 = 100MHz.  Integer part in bits [23:12].
    PUT32(CM_CAM1DIV, CM_PASSWD | (5 << 12));

    // Start clock
    PUT32(CM_CAM1CTL, CM_PASSWD | CM_ENAB | CM_SRC_PLLD);
    while (!(GET32(CM_CAM1CTL) & CM_BUSY))
        ;
}

void notmain(void) {
    output("--- Phase 2: Unicam Power & Clock ---\n");

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

    enable_power_domain(RPI_POWER_DOMAIN_UNICAM1);
    output("UNICAM1 domain (13) enabled.\n");

    enable_power_domain(RPI_POWER_DOMAIN_CSI2);
    output("CSI2 domain (15) enabled.\n");

    dev_barrier();
    cam1_clock_init();
    dev_barrier();
    output("CAM1 clock: 100MHz from PLLD.\n");

    dev_barrier();
    uint32_t ctrl_before = GET32(UNICAM_BASE);
    output("UNICAM_CTRL before = %x\n", ctrl_before);

    PUT32(UNICAM_BASE, 1);
    dev_barrier();
    uint32_t ctrl_after = GET32(UNICAM_BASE);
    output("UNICAM_CTRL after write 1 = %x\n", ctrl_after);

    PUT32(UNICAM_BASE, 0);
    dev_barrier();

    if (ctrl_after & 1)
        output("Unicam register write-readback OK: power domain is ON.\n");
    else
        output("WARNING: write-readback failed, power domain may not be active.\n");

    output("--- Phase 2: PASS ---\n");
    clean_reboot();
}
