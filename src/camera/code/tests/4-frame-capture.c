// Phase 4: Unicam CSI-2 receiver + DMA + frame capture.
// Configures Unicam, allocates GPU buffer, captures one frame.
// Includes detailed diagnostics for debugging camera pipeline.
#include "rpi.h"
#include "imx477.h"
#include "unicam.h"
#include "frame.h"

static const char hexc[] = "0123456789abcdef";

// Output a full-resolution crop as packed RAW12 hex.
// Preserves the Bayer pattern for proper debayering on the host.
static void dump_raw_crop(volatile uint8_t *buf,
                          unsigned cx, unsigned cy,
                          unsigned cw, unsigned ch) {
    cx &= ~1u;
    cy &= ~1u;
    cw &= ~1u;
    ch &= ~1u;

    unsigned bytes_per_line = (cw * 3) / 2;

    output("---RAW12 %d %d %d %d---\n", cw, ch, cx, cy);
    for (unsigned y = cy; y < cy + ch && y < FRAME_HEIGHT; y++) {
        volatile uint8_t *line = buf + (unsigned)y * FRAME_LINE_STRIDE;
        unsigned start = (cx / 2) * 3;
        for (unsigned b = 0; b < bytes_per_line; b++) {
            uint8_t val = line[start + b];
            output("%c%c", hexc[val >> 4], hexc[val & 0xF]);
        }
        output("\n");
    }
    output("---END---\n");
}

// Read sensor registers for debugging.
static void sensor_diag(void) {
    uint8_t val;

    imx477_read_reg8(0x0100, &val);
    output("  MODE_SELECT=%x", val);

    imx477_read_reg8(0x0114, &val);
    output(" LANE_MODE=%x", val);

    imx477_read_reg8(0x0005, &val);
    output(" FRAME_CNT=%x", val);

    imx477_read_reg8(0x0808, &val);
    output(" DPHY_CTRL=%x", val);

    imx477_read_reg8(0x0601, &val);
    output(" TEST_PAT=%x\n", val);
}

void notmain(void) {
    output("--- Phase 4: Frame Capture ---\n");

    // Initialize camera pipeline + GPU buffer.
    frame_t f;
    frame_init(&f);

    // Snapshot lane states before capture.
    output("Pre-init: CAM1_CLK=%x CAM1_DAT0=%x\n",
           GET32(UNICAM_BASE + UNICAM_CLK), GET32(UNICAM_BASE + UNICAM_DAT0));
    output("Pre-init: CAM0_CLK=%x CAM0_DAT0=%x\n",
           GET32(0x20800000 + 0x010), GET32(0x20800000 + 0x018));

    // Capture one frame.
    frame_capture(&f);
    output("Capture result=%d\n", f.result);
    output("IBWP=%x (wrote %d bytes, ~%d lines at stride %d)\n",
        f.ibwp, f.ibwp - f.bus_addr,
        (f.ibwp - f.bus_addr) / FRAME_LINE_STRIDE, FRAME_LINE_STRIDE);

    if (f.result == -1)
        output("TIMEOUT: no frame captured.\n");
    else if (f.result == -2)
        output("ERROR: Unicam reported FIFO/sync error.\n");
    else if (f.result == -3)
        output("WARNING: frame captured but has data loss.\n");
    else
        output("Frame captured cleanly.\n");

    // Hex dump: first 48 bytes.
    output("Buffer dump (first 48 bytes):\n");
    for (unsigned i = 0; i < 48; i += 16) {
        output("%x: ", i);
        for (unsigned j = 0; j < 16; j++)
            output("%x ", f.buf[i + j]);
        output("\n");
    }

    // Sample at color bar boundaries.
    output("Color bar samples (line 0, every 380 bytes):\n");
    for (unsigned bar = 0; bar < 8; bar++) {
        unsigned off = bar * 380;
        output("  bar%d @%x: %x %x %x %x %x %x\n", bar, off,
            f.buf[off], f.buf[off+1], f.buf[off+2],
            f.buf[off+3], f.buf[off+4], f.buf[off+5]);
    }

    // Sample at line boundaries.
    output("Line start samples (every 3056 bytes):\n");
    for (unsigned line = 0; line < 8; line++) {
        unsigned off = line * FRAME_LINE_STRIDE;
        output("  line%d @%x: %x %x %x %x %x %x\n", line, off,
            f.buf[off], f.buf[off+1], f.buf[off+2],
            f.buf[off+3], f.buf[off+4], f.buf[off+5]);
    }

    // DL boundary diagnostic.
    unsigned dl_line = (f.ibwp - f.bus_addr) / FRAME_LINE_STRIDE;
    output("DL boundary diagnostic (dl_line=%d):\n", dl_line);
    unsigned check_lines[] = {0, dl_line-2, dl_line-1, dl_line, dl_line+1,
                              dl_line+10, dl_line+100, 1000, 1519};
    for (unsigned i = 0; i < 9; i++) {
        unsigned ln = check_lines[i];
        if (ln >= FRAME_ALLOC_SIZE / FRAME_LINE_STRIDE) continue;
        unsigned off = ln * FRAME_LINE_STRIDE;
        int nonz = 0;
        for (unsigned j = 0; j < 32; j++)
            if (f.buf[off + j] != 0) { nonz = 1; break; }
        output("  line%d @%x: %x %x %x %x %x %x %s\n", ln, off,
            f.buf[off], f.buf[off+1], f.buf[off+2],
            f.buf[off+3], f.buf[off+4], f.buf[off+5],
            nonz ? "DATA" : "ZERO");
    }

    // Check for non-zero data.
    int nonzero = 0;
    for (unsigned i = 0; i < 256; i++) {
        if (f.buf[i] != 0) { nonzero = 1; break; }
    }

    if ((f.result == 0 || f.result == -3) && nonzero)
        output("--- Phase 4: PASS ---\n");
    else if (f.result == 0)
        output("--- Phase 4: FAIL (frame captured but buffer is zero) ---\n");
    else
        output("--- Phase 4: FAIL ---\n");

    // Save frame to SD card.
    if ((f.result == 0 || f.result == -3) && nonzero) {
        output("Saving frame to SD card...\n");
        frame_sd_init();
        if (frame_save(&f, "FRAME.RAW"))
            output("FRAME.RAW written to SD card (%d bytes).\n", FRAME_BUF_SIZE);
        else
            output("ERROR: frame save failed!\n");
    }

    frame_cleanup(&f);
    clean_reboot();
}
