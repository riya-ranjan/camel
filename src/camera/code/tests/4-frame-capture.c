// Phase 4: Unicam CSI-2 receiver + DMA + frame capture.
// Configures Unicam, allocates GPU buffer, captures one frame of color bars.
// Outputs image data over UART for host-side processing.
#include "rpi.h"
#include "mbox.h"
#include "cam-power.h"
#include "i2c.h"
#include "imx477.h"
#include "unicam.h"
#include "pi-sd.h"
#include "mbr.h"
#include "fat32.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477,
    BSC0_BASE          = 0x20205000,
};

// Image parameters: 2028x1520, packed RAW12.
enum {
    IMG_WIDTH   = 2028,
    IMG_HEIGHT  = 1520,
    LINE_STRIDE = 3056,     // ceil(2028*1.5 / 16) * 16
    BUF_SIZE    = 3056 * 1520,  // 4,645,120 bytes
    ALLOC_SIZE  = 5 * 1024 * 1024,  // 5MB
    SUBSAMPLE   = 8,        // 8x downsample for UART transfer
};

static const char hexc[] = "0123456789abcdef";

// Output a full-resolution crop as packed RAW12 hex.
// This preserves the Bayer pattern for proper debayering on the host.
// Crop size must be even (for Bayer alignment).
static void dump_raw_crop(volatile uint8_t *buf,
                          unsigned cx, unsigned cy,
                          unsigned cw, unsigned ch) {
    // Ensure even alignment for Bayer pattern.
    cx &= ~1u;
    cy &= ~1u;
    cw &= ~1u;
    ch &= ~1u;

    // Bytes per crop line in packed RAW12: cw * 1.5.
    unsigned bytes_per_line = (cw * 3) / 2;

    output("---RAW12 %d %d %d %d---\n", cw, ch, cx, cy);
    for (unsigned y = cy; y < cy + ch && y < IMG_HEIGHT; y++) {
        volatile uint8_t *line = buf + (unsigned)y * LINE_STRIDE;
        // Starting byte offset for pixel cx in packed RAW12.
        unsigned start = (cx / 2) * 3;
        for (unsigned b = 0; b < bytes_per_line; b++) {
            uint8_t val = line[start + b];
            output("%c%c", hexc[val >> 4], hexc[val & 0xF]);
        }
        output("\n");
    }
    output("---END---\n");
}

// Check if the sensor is actively generating frames by reading
// registers during streaming.
static void sensor_diag(void) {
    uint8_t val;

    // MODE_SELECT should still be 1 (streaming).
    imx477_read_reg8(0x0100, &val);
    output("  MODE_SELECT=%x", val);

    // CSI_LANE_MODE: should be 1 (2 lanes).
    imx477_read_reg8(0x0114, &val);
    output(" LANE_MODE=%x", val);

    // Frame count register (IMX477: 0x0005).
    imx477_read_reg8(0x0005, &val);
    output(" FRAME_CNT=%x", val);

    // DPHY control register.
    imx477_read_reg8(0x0808, &val);
    output(" DPHY_CTRL=%x", val);

    // Readback test pattern.
    imx477_read_reg8(0x0601, &val);
    output(" TEST_PAT=%x\n", val);
}

void notmain(void) {
    output("--- Phase 4: Frame Capture ---\n");

    // System clocks — boost core (VPU) for more SDRAM bandwidth.
    output("Core clock: %d Hz\n", rpi_clock_curhz_get(MBOX_CLK_CORE));
    output("Max core clock: %d Hz\n", rpi_clock_maxhz_get(MBOX_CLK_CORE));
    rpi_clock_hz_set(MBOX_CLK_CORE, 400 * 1000 * 1000);
    output("Core clock after boost: %d Hz\n", rpi_clock_curhz_get(MBOX_CLK_CORE));
    output("ARM clock: %d Hz\n", rpi_clock_curhz_get(MBOX_CLK_CPU));

    // 1. Power on camera.
    cam_power_on();
    output("Camera powered on.\n");

    // 2. Init I2C.
    i2c_init_ex(BSC0_BASE, 28, 29);

    // 3. Verify chip ID.
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

    // 4. Enable Unicam power domains + CAM1 clock.
    unicam_power_init();

    // Also try enabling UNICAM0 power domain (firmware ID 13 = DT index 12 + 1).
    {
        volatile uint32_t msg[8] __attribute__((aligned(16)));
        msg[0] = 8 * 4; msg[1] = 0;
        msg[2] = 0x00038030; msg[3] = 8; msg[4] = 0;
        msg[5] = 13; msg[6] = 1; msg[7] = 0;
        mbox_send(MBOX_CH, msg);
        output("UNICAM0 domain (fw=13) also enabled.\n");
    }

    // 5. Full sensor init (common + mode, stays in standby).
    imx477_init();
    output("Sensor init done.\n");

    // 6. No test pattern — capture real image.
    imx477_set_test_pattern(0);
    output("Test pattern: OFF (real image).\n");

    // 7-8. Allocate and lock GPU memory for DMA buffer.
    uint32_t handle = gpu_mem_alloc(ALLOC_SIZE, 4096,
                        GPU_MEM_FLAG_DIRECT | GPU_MEM_FLAG_ZERO);
    if (!handle) panic("gpu_mem_alloc failed!\n");

    uint32_t bus_addr = gpu_mem_lock(handle);
    if (!bus_addr) panic("gpu_mem_lock failed!\n");
    output("GPU buffer: handle=%x bus_addr=%x (%d bytes)\n",
           handle, bus_addr, ALLOC_SIZE);

    // Snapshot lane states BEFORE arming Unicam.
    output("Pre-init: CAM1_CLK=%x CAM1_DAT0=%x\n",
           GET32(0x20801000 + 0x010), GET32(0x20801000 + 0x018));
    output("Pre-init: CAM0_CLK=%x CAM0_DAT0=%x\n",
           GET32(0x20800000 + 0x010), GET32(0x20800000 + 0x018));

    // 9. Configure Unicam receiver (use full allocation for DMA headroom).
    unicam_rx_init(bus_addr, ALLOC_SIZE, LINE_STRIDE);

    // 10. Start sensor streaming — go straight to frame wait, NO output()
    //     between start and frame capture to avoid any bus contention.
    imx477_start_streaming();

    // 11. Wait for a clean frame (1s timeout).
    //     unicam_wait_frame skips the first frame, reloads buffer in the
    //     vertical blanking gap, then captures the second frame cleanly.
    int result = unicam_wait_frame(1000000);
    output("Capture result=%d\n", result);

    // 12. Read IBWP BEFORE stopping anything (clock gate reset clears it).
    dev_barrier();
    uint32_t ibwp = GET32(UNICAM_BASE + 0x11C);  // IBWP
    output("IBWP=%x (wrote %d bytes, ~%d lines at stride %d)\n",
        ibwp, ibwp - bus_addr, (ibwp - bus_addr) / LINE_STRIDE, LINE_STRIDE);

    // 13. Stop sensor.
    imx477_stop_streaming();

    // 14. Stop Unicam.
    unicam_stop();

    if (result == -1) {
        output("TIMEOUT: no frame captured.\n");
    } else if (result == -2) {
        output("ERROR: Unicam reported FIFO/sync error.\n");
    } else if (result == -3) {
        output("WARNING: frame captured but has data loss.\n");
    } else {
        output("Frame captured cleanly.\n");
    }

    // 15. Convert bus address to ARM physical for reading.
    volatile uint8_t *buf = (volatile uint8_t *)(bus_addr & 0x3FFFFFFF);

    // 15. Hex dump: first 48 bytes, then samples every ~380 bytes (color bar width).
    output("Buffer dump (first 48 bytes):\n");
    for (unsigned i = 0; i < 48; i += 16) {
        output("%x: ", i);
        for (unsigned j = 0; j < 16; j++)
            output("%x ", buf[i + j]);
        output("\n");
    }
    // Sample at color bar boundaries (2028/8 bars = ~253px, ~380 bytes each).
    output("Color bar samples (line 0, every 380 bytes):\n");
    for (unsigned bar = 0; bar < 8; bar++) {
        unsigned off = bar * 380;
        output("  bar%d @%x: %x %x %x %x %x %x\n", bar, off,
            buf[off], buf[off+1], buf[off+2],
            buf[off+3], buf[off+4], buf[off+5]);
    }
    // Sample at line boundaries.
    output("Line start samples (every 3056 bytes):\n");
    for (unsigned line = 0; line < 8; line++) {
        unsigned off = line * LINE_STRIDE;
        output("  line%d @%x: %x %x %x %x %x %x\n", line, off,
            buf[off], buf[off+1], buf[off+2],
            buf[off+3], buf[off+4], buf[off+5]);
    }

    // Diagnostic: check data around and beyond the DL boundary.
    unsigned dl_line = (ibwp - bus_addr) / LINE_STRIDE;
    output("DL boundary diagnostic (dl_line=%d):\n", dl_line);
    unsigned check_lines[] = {0, dl_line-2, dl_line-1, dl_line, dl_line+1,
                              dl_line+10, dl_line+100, 1000, 1519};
    for (unsigned i = 0; i < 9; i++) {
        unsigned ln = check_lines[i];
        if (ln >= ALLOC_SIZE / LINE_STRIDE) continue;
        unsigned off = ln * LINE_STRIDE;
        // Check if the entire line is zero.
        int nonz = 0;
        for (unsigned j = 0; j < 32; j++)
            if (buf[off + j] != 0) { nonz = 1; break; }
        output("  line%d @%x: %x %x %x %x %x %x %s\n", ln, off,
            buf[off], buf[off+1], buf[off+2],
            buf[off+3], buf[off+4], buf[off+5],
            nonz ? "DATA" : "ZERO");
    }

    // 16. Check if buffer has non-zero data.
    int nonzero = 0;
    for (unsigned i = 0; i < 256; i++) {
        if (buf[i] != 0) { nonzero = 1; break; }
    }

    if ((result == 0 || result == -3) && nonzero)
        output("--- Phase 4: PASS ---\n");
    else if (result == 0)
        output("--- Phase 4: FAIL (frame captured but buffer is zero) ---\n");
    else
        output("--- Phase 4: FAIL ---\n");

    // 17. Save frame to SD card as FRAME.RAW.
    if ((result == 0 || result == -3) && nonzero) {
        output("Saving frame to SD card...\n");

        // Init heap for FAT32 (needs kmalloc).
        kmalloc_init();

        // Init SD card.
        pi_sd_init();
        output("SD card initialized.\n");

        // Read MBR, load FAT32.
        mbr_t *mbr = mbr_read();
        mbr_partition_ent_t partition;
        memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
        fat32_fs_t fs = fat32_mk(&partition);
        pi_dirent_t root = fat32_get_root(&fs);
        output("FAT32 filesystem mounted.\n");

        // Delete old file if it exists (ignore errors).
        fat32_delete(&fs, &root, "FRAME.RAW");

        // Create new file.
        pi_dirent_t *created = fat32_create(&fs, &root, "FRAME.RAW", 0);
        if (!created)
            output("WARNING: fat32_create failed!\n");
        else {
            // Write frame data.
            pi_file_t frame_file = {
                .data = (char *)buf,
                .n_data = BUF_SIZE,
                .n_alloc = BUF_SIZE,
            };
            int wr = fat32_write(&fs, &root, "FRAME.RAW", &frame_file);
            if (wr)
                output("FRAME.RAW written to SD card (%d bytes).\n", BUF_SIZE);
            else
                output("ERROR: fat32_write failed!\n");
        }
    }

    // 18. Cleanup.
    gpu_mem_unlock(handle);
    gpu_mem_free(handle);

    clean_reboot();
}
