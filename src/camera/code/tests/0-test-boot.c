// Phase 0: verify the Pi boots with camera firmware (dt-blob.bin)
#include "rpi.h"
#include "mbox.h"

void notmain(void) {
    output("--- Phase 0: boot test with camera firmware ---\n");

    output("serial number  = %llx\n", rpi_get_serialnum());
    output("revision       = %x\n", rpi_get_revision());
    output("model          = %x\n", rpi_get_model());

    uint32_t size = rpi_get_memsize();
    output("physical mem   = %d bytes (%dMB)\n", size, size / (1024 * 1024));

    unsigned temp = rpi_temp_get();
    unsigned C = temp / 1000, F = C * 9 / 5 + 32;
    output("temperature    = %dC (%dF)\n", C, F);

    uint32_t arm_cur = rpi_clock_curhz_get(MBOX_CLK_CPU);
    uint32_t arm_max = rpi_clock_maxhz_get(MBOX_CLK_CPU);
    output("ARM clock      = %dHz (max %dHz)\n", arm_cur, arm_max);

    output("--- Phase 0: PASS ---\n");
}
