// Unicam CSI-2 receiver driver for BCM2835 (CAM1).
// Ported from Linux vc4/bcm2835-unicam driver (unicam_start_rx).
//
// Register bit definitions from vc4-regs-unicam.h (rpi-5.10.y):
// https://github.com/raspberrypi/linux/blob/rpi-6.6.y/drivers/media/platform/bcm2835/vc4-regs-unicam.h
#include "rpi.h"
#include "mbox.h"
#include "unicam.h"

static inline uint32_t unicam_rd(uint32_t off) {
    return GET32(UNICAM_BASE + off);
}
static inline void unicam_wr(uint32_t off, uint32_t val) {
    PUT32(UNICAM_BASE + off, val);
}

// power domain IDs ported from raspberrypi-power.c
enum {
    RPI_POWER_DOMAIN_UNICAM1  = 14,    // DT index 13 + 1
    RPI_POWER_DOMAIN_CSI2     = 16,    // DT index 15 + 1
};

enum {
    CM_CAM1CTL = 0x20101048,
    CM_CAM1DIV = 0x2010104C,
    CM_PASSWD  = 0x5A000000,
    CM_ENAB    = 1 << 4,
    CM_BUSY    = 1 << 7,
    CM_SRC_PLLD = 6,       // PLLD_PER = 500MHz
};

enum {
    CTRL_CPE = 1 << 0,     // capture Peripheral Enable
    CTRL_MEM = 1 << 1,     // memory Enable
    CTRL_CPR = 1 << 2,     // capture Peripheral Reset
    CTRL_SOE = 1 << 4,     // Stop Output Engine
};


static void cam1_clock_init(void) {
    PUT32(CM_CAM1CTL, CM_PASSWD | CM_SRC_PLLD);
    while (GET32(CM_CAM1CTL) & CM_BUSY)
        ;
    PUT32(CM_CAM1DIV, CM_PASSWD | (5 << 12));
    PUT32(CM_CAM1CTL, CM_PASSWD | CM_ENAB | CM_SRC_PLLD);
    while (!(GET32(CM_CAM1CTL) & CM_BUSY))
        ;
}

void unicam_power_init(void) {
    rpi_domain_enable(RPI_POWER_DOMAIN_UNICAM1);
    output("UNICAM1 domain (13) enabled.\n");

    rpi_domain_enable(RPI_POWER_DOMAIN_CSI2);
    output("CSI2 domain (15) enabled.\n");

    dev_barrier();
    cam1_clock_init();
    dev_barrier();
    output("CAM1 clock: 100MHz from PLLD.\n");
}

void unicam_rx_init(uint32_t bus_addr, uint32_t buf_size, uint32_t stride) {
    uint32_t val;

    // line 2325 -> 2329, linux bcm2835-unicam.c
    PUT32(UNICAM_CLK_GATE, 0x5A000015);
    dev_barrier();

    // basic initialization
    // line 2331 -> 2332, linux bcm2835-unicam.c
    unicam_wr(UNICAM_CTRL, CTRL_MEM);
    dev_barrier();

    // enable analog control
    // line 2334 -> 2339, linux bcm2835-unicam.c
    unicam_wr(UNICAM_ANA, 0x00000774);
    delay_ms(1);

    // release analog reset (clear AR bit 2)
    // line 2341 -> 2342, linux bcm2835-unicam.c
    unicam_wr(UNICAM_ANA, 0x00000770);
    dev_barrier();

    // peripheral reset
    // line 2344 -> 2348 linux bcm2835-unicam.c
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val | CTRL_CPR);
    dev_barrier();
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val & ~CTRL_CPR);
    dev_barrier();

    // CTRL: MEM(1) + CPM=0(CSI-2) + PFT=0([11:8]) + OET=0x80([20:12]).
    // enable rx control / packet framer timeout 
    // line 2351 -> 2362 linux bcm2835-unicam.c
    unicam_wr(UNICAM_CTRL, 0x00080002);
    dev_barrier();

    // line 2364 -> 2365 linux bcm2835-unicam.c
    unicam_wr(UNICAM_IHWIN, 0);
    unicam_wr(UNICAM_IVWIN, 0);

    // AXI QoS: PE=1, PT=2, NP=8, PP=14
    // line 2368 -> 2375 linux bcm2835-unicam.c
    unicam_wr(UNICAM_PRI, 0x00000E85);

    // line 2377 linux bcm2835-unicam.c
    val = unicam_rd(UNICAM_ANA);
    unicam_wr(UNICAM_ANA, val & ~(1 << 3));
    dev_barrier();

    // ICTL: FSIE(0) + FEIE(1), LCIE=380.
    // we aren't doing video streaming so this is okay
    // line 2381 linux bcm2835-unicam.c
    unicam_wr(UNICAM_ICTL, 0x017C0003);

    // line 2382 -> 2383 linux bcm2835-unicam.c
    unicam_wr(UNICAM_STA, 0xFFFFFFFF);
    unicam_wr(UNICAM_ISTA, 0xFFFFFFFF);

    // line 2385 -> 2388 linux bcm2835-unicam.c
    unicam_wr(UNICAM_CLT, 0x00000602);

    // line 2390 -> 2394 linux bcm2835-unicam.c
    unicam_wr(UNICAM_DLT, 0x00000602);

    // linx 2396 linux bcm2835-unicam.c
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val & ~CTRL_SOE);
    dev_barrier();

    // line 2399 -> 2405 linux bcm2835-unicam.c
    unicam_wr(UNICAM_CMP0, 0x80000301);

    // line 2410 -> 2415 linux bcm2835-unicam.c
    unicam_wr(UNICAM_CLK, 0x0000001D);

    // line 2445 -> 2449 linux bcm2835-unicam.c
    // IMX477 only uses 2 data lanes
    unicam_wr(UNICAM_DAT0, 0x0000001D);
    unicam_wr(UNICAM_DAT1, 0x0000001D);

    // line 2465 -> 2466 linux bcm2835-unicam.c
    unicam_wr(UNICAM_IBLS, stride);

    // line 2470 linux bcm2835-unicam.c
    unicam_wr(UNICAM_IBSA0, bus_addr);
    unicam_wr(UNICAM_IBEA0, bus_addr + buf_size);

    // we just want the raw data as is
    unicam_wr(UNICAM_IPIPE, 0x00000000);

    // format as raw12 bayer instead of raw10 bayer
    unicam_wr(UNICAM_IDI0, 0x0000002C);

    // line 2477 - 2481 linux bcm2835-unicam.c (we don't use embedded data)
    unicam_wr(UNICAM_DCS, 0x00000000);

    // line 2472 -> 2475 linux bcm2835-unicam.c
    val = unicam_rd(UNICAM_MISC);
    unicam_wr(UNICAM_MISC, val | (1 << 6) | (1 << 9));
    dev_barrier();

    // enable unicam
    // line 2484 linux bcm2835-unicam.c
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val | CTRL_CPE);
    dev_barrier();

    // line 2487 linux bcm2835-unicam.c
    val = unicam_rd(UNICAM_ICTL);
    unicam_wr(UNICAM_ICTL, val | (1 << 5) | (1 << 4));
    dev_barrier();

    output("Unicam RX configured (continuous mode).\n");
    output("  CTRL=%x ICTL=%x CLK=%x DAT0=%x\n",
        unicam_rd(UNICAM_CTRL), unicam_rd(UNICAM_ICTL),
        unicam_rd(UNICAM_CLK), unicam_rd(UNICAM_DAT0));
    output("  IBSA0=%x IBEA0=%x IBLS=%x\n",
        unicam_rd(UNICAM_IBSA0), unicam_rd(UNICAM_IBEA0),
        unicam_rd(UNICAM_IBLS));
}

int unicam_wait_frame(uint32_t timeout_us) {
    uint32_t start = timer_get_usec();

    unicam_wr(UNICAM_STA, 0xFFFFFFFF);      // clear prior status bits
    unicam_wr(UNICAM_ISTA, 0xFFFFFFFF);
    dev_barrier();

    // wait for frame start bit to be set (bit 0 of ISTA)
    // taken from line 176 of linux vc4-regs-unicam.h
    while (!(unicam_rd(UNICAM_ISTA) & (1 << 0))) {
        if (timer_get_usec() - start > timeout_us) {
            output("TIMEOUT waiting for FSI: STA=%x ISTA=%x\n",
                unicam_rd(UNICAM_STA), unicam_rd(UNICAM_ISTA));
            return -1;
        }
    }
    output("  FSI at %d us\n", timer_get_usec() - start);

    // clear prior status bits
    unicam_wr(UNICAM_ISTA, 0xFFFFFFFF);
    dev_barrier();

    // we wait for the second frame start interrupt to indicate that the frame is complete
    while (!(unicam_rd(UNICAM_ISTA) & (1 << 0))) {
        uint32_t sta = unicam_rd(UNICAM_STA);

        // detect any fifo errors/throw error
        if (sta & ((1 << 9) | (1 << 10) | (1 << 11))) {
            output("Unicam FIFO error: STA=%x ISTA=%x\n",
                sta, unicam_rd(UNICAM_ISTA));
            return -2;
        }

        if (timer_get_usec() - start > timeout_us) {
            output("TIMEOUT waiting for frame end: STA=%x ISTA=%x IBWP=%x\n",
                unicam_rd(UNICAM_STA), unicam_rd(UNICAM_ISTA),
                unicam_rd(UNICAM_IBWP));
            return -1;
        }
    }

    uint32_t sta = unicam_rd(UNICAM_STA);
    uint32_t ista = unicam_rd(UNICAM_ISTA);
    uint32_t elapsed = timer_get_usec() - start;
    output("  Frame complete at %d us, STA=%x ISTA=%x\n", elapsed, sta, ista);

    if (sta & (1 << 12)) {
        output("  WARNING: DL (Data Lost) set\n");
        return -3;
    }

    // write to clear
    unicam_wr(UNICAM_ISTA, ista);
    return 0;
}

void unicam_stop(void) {
    uint32_t val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val & ~CTRL_CPE);    // turn cam peripheral enable bit off, line 74 linux vc4-regs-unicam.h
    dev_barrier();

    PUT32(UNICAM_CLK_GATE, 0x5A000000);         // disable active data lanes (inverse of setup)
    dev_barrier();
}
