// Unicam CSI-2 receiver driver for BCM2835 (CAM1).
// Ported from Linux vc4/bcm2835-unicam driver (unicam_start_rx).
//
// Register bit definitions from vc4-regs-unicam.h (rpi-5.10.y):
//   CTRL: CPE=0, MEM=1, CPR=2, CPM=3(0=CSI2), SOE=4, DCM=5,
//         PFT=[11:8], OET=[20:12]
//   ANA:  APD=0, BPD=1, AR=2, DDL=3, CTATADJ=[7:4], PTATADJ=[11:8]
//   ICTL: FSIE=0, FEIE=1, IBOB=2, FCM=3, TFC=4, LIP=[6:5], LCIE=[28:16]
#include "rpi.h"
#include "mbox.h"
#include "unicam.h"

static inline uint32_t unicam_rd(uint32_t off) {
    return GET32(UNICAM_BASE + off);
}
static inline void unicam_wr(uint32_t off, uint32_t val) {
    PUT32(UNICAM_BASE + off, val);
}

// Mailbox tag for SET_DOMAIN_STATE.
// IMPORTANT: firmware domain ID = DT binding index + 1.
// DT binding (raspberrypi-power.h): UNICAM1=13, CSI2=15
// Firmware IDs: UNICAM1=14, CSI2=16
// See: raspberrypi-power.c: dom->domain = i + 1;
enum {
    MBOX_TAG_SET_DOMAIN_STATE = 0x00038030,
    RPI_POWER_DOMAIN_UNICAM1  = 14,    // DT index 13 + 1
    RPI_POWER_DOMAIN_CSI2     = 16,    // DT index 15 + 1
};

// Clock manager registers for CAM1.
enum {
    CM_CAM1CTL = 0x20101048,
    CM_CAM1DIV = 0x2010104C,
    CM_PASSWD  = 0x5A000000,
    CM_ENAB    = 1 << 4,
    CM_BUSY    = 1 << 7,
    CM_SRC_PLLD = 6,       // PLLD_PER = 500MHz
};

// CTRL register bits.
enum {
    CTRL_CPE = 1 << 0,     // Capture Peripheral Enable
    CTRL_MEM = 1 << 1,     // Memory Enable
    CTRL_CPR = 1 << 2,     // Capture Peripheral Reset
    // CPM = bit 3: 0=CSI-2, 1=CCP2
    CTRL_SOE = 1 << 4,     // Stop Output Engine
};

static void enable_power_domain(unsigned domain) {
    volatile uint32_t msg[8] __attribute__((aligned(16)));
    msg[0] = 8 * 4;
    msg[1] = 0;
    msg[2] = MBOX_TAG_SET_DOMAIN_STATE;
    msg[3] = 8;
    msg[4] = 0;
    msg[5] = domain;
    msg[6] = 1;     // ON
    msg[7] = 0;     // end tag
    mbox_send(MBOX_CH, msg);
}

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
    enable_power_domain(RPI_POWER_DOMAIN_UNICAM1);
    output("UNICAM1 domain (13) enabled.\n");

    enable_power_domain(RPI_POWER_DOMAIN_CSI2);
    output("CSI2 domain (15) enabled.\n");

    dev_barrier();
    cam1_clock_init();
    dev_barrier();
    output("CAM1 clock: 100MHz from PLLD.\n");
}

void unicam_rx_init(uint32_t bus_addr, uint32_t buf_size, uint32_t stride) {
    uint32_t val;

    // 1. Clock gate: enable clock + 2 data lanes.
    //    Linux: val=1; for each lane: val=(val<<2)|1. 2 lanes → 0x15
    PUT32(UNICAM_CLK_GATE, 0x5A000015);
    dev_barrier();

    // 2. CTRL = MEM only.
    unicam_wr(UNICAM_CTRL, CTRL_MEM);
    dev_barrier();

    // 3. ANA: analog reset + CTATADJ=7 + PTATADJ=7.
    unicam_wr(UNICAM_ANA, 0x00000774);
    delay_ms(1);

    // 4. Release analog reset (clear AR bit 2).
    unicam_wr(UNICAM_ANA, 0x00000770);
    dev_barrier();

    // 5-6. Peripheral reset pulse (CPR = bit 2).
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val | CTRL_CPR);
    dev_barrier();
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val & ~CTRL_CPR);
    dev_barrier();

    // 7. CTRL: MEM(1) + CPM=0(CSI-2) + PFT=0([11:8]) + OET=0x80([20:12]).
    unicam_wr(UNICAM_CTRL, 0x00080002);
    dev_barrier();

    // 8-9. No crop.
    unicam_wr(UNICAM_IHWIN, 0);
    unicam_wr(UNICAM_IVWIN, 0);

    // 10. AXI QoS: PE=1, PT=2, NP=8, PP=14 (matches Linux driver).
    //     Without priority, Unicam gets starved by GPU on shared SDRAM bus.
    unicam_wr(UNICAM_PRI, 0x00000E85);

    // 11. Clear DDL (bit 3 in ANA).
    val = unicam_rd(UNICAM_ANA);
    unicam_wr(UNICAM_ANA, val & ~(1 << 3));
    dev_barrier();

    // 12. ICTL: FSIE(0) + FEIE(1), LCIE=380.
    //     No IBOB — single frame capture, prevents frame 2 from overwriting frame 1.
    unicam_wr(UNICAM_ICTL, 0x017C0003);

    // 13-14. Clear all status.
    unicam_wr(UNICAM_STA, 0xFFFFFFFF);
    unicam_wr(UNICAM_ISTA, 0xFFFFFFFF);

    // 15. D-PHY clock lane timing (Linux hard-codes these).
    unicam_wr(UNICAM_CLT, 0x00000602);

    // 16. D-PHY data lane timing.
    unicam_wr(UNICAM_DLT, 0x00000602);

    // 17. Clear SOE (bit 4).
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val & ~CTRL_SOE);
    dev_barrier();

    // 18. Packet compare: PCE(31) + GI(9) + CPH(8) + PCDT=1(FS).
    unicam_wr(UNICAM_CMP0, 0x80000301);

    // 19. Clock lane: CLE(0)+CLLPE(2)+CLHSE(3)+CLTRE(4) = 0x1D (continuous clock).
    // IMX477 uses continuous clock mode by default.
    unicam_wr(UNICAM_CLK, 0x0000001D);

    // 20-21. Data lanes: DLE(0)+DLLPE(2)+DLHSE(3)+DLTRE(4) = 0x1D.
    unicam_wr(UNICAM_DAT0, 0x0000001D);
    unicam_wr(UNICAM_DAT1, 0x0000001D);

    // 22. Line stride.
    unicam_wr(UNICAM_IBLS, stride);

    // 23-24. DMA buffer.
    unicam_wr(UNICAM_IBSA0, bus_addr);
    unicam_wr(UNICAM_IBEA0, bus_addr + buf_size);

    // 25. Image pipeline: raw passthrough (DEBL=0, PUM=0, PPM=0).
    //     With IPIPE=0, FEI won't fire — we detect frame end via second FSI.
    unicam_wr(UNICAM_IPIPE, 0x00000000);

    // 26. IDI0: VC=0, DT=0x2C (RAW12).
    unicam_wr(UNICAM_IDI0, 0x0000002C);

    // 27. Disable embedded data.
    unicam_wr(UNICAM_DCS, 0x00000000);

    // 28. MISC: flush channels FL0(6) + FL1(9).
    val = unicam_rd(UNICAM_MISC);
    unicam_wr(UNICAM_MISC, val | (1 << 6) | (1 << 9));
    dev_barrier();

    // 29. Enable peripheral (CPE).
    val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val | CTRL_CPE);
    dev_barrier();

    // 30. Load image pointers (LIP=bit5) + trigger first capture (TFC=bit4).
    //     Linux sets both after enabling CPE.
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

    // 1. Clear all status from any prior activity.
    unicam_wr(UNICAM_STA, 0xFFFFFFFF);
    unicam_wr(UNICAM_ISTA, 0xFFFFFFFF);
    dev_barrier();

    // 2. Wait for FSI (Frame Start, ISTA bit 0).
    //    TFC was set in unicam_rx_init, so the DMA engine is armed and
    //    waiting for this FS packet. DMA starts from IBSA0.
    while (!(unicam_rd(UNICAM_ISTA) & (1 << 0))) {
        if (timer_get_usec() - start > timeout_us) {
            output("TIMEOUT waiting for FSI: STA=%x ISTA=%x\n",
                unicam_rd(UNICAM_STA), unicam_rd(UNICAM_ISTA));
            return -1;
        }
    }
    output("  FSI at %d us\n", timer_get_usec() - start);

    // 3. Clear FSI. Frame data is now being DMA'd into the buffer.
    unicam_wr(UNICAM_ISTA, 0xFFFFFFFF);
    dev_barrier();

    // 4. Wait for second FSI (next frame start = current frame complete).
    //    With IPIPE=0 (raw passthrough), FEI is not generated. We detect
    //    frame completion when the next FS packet arrives (~95ms later).
    while (!(unicam_rd(UNICAM_ISTA) & (1 << 0))) {
        uint32_t sta = unicam_rd(UNICAM_STA);

        // FIFO errors: IFO(9), OFO(10), BFO(11).
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

    unicam_wr(UNICAM_ISTA, ista);
    return 0;
}

void unicam_stop(void) {
    uint32_t val = unicam_rd(UNICAM_CTRL);
    unicam_wr(UNICAM_CTRL, val & ~CTRL_CPE);
    dev_barrier();

    PUT32(UNICAM_CLK_GATE, 0x5A000000);
    dev_barrier();
}
