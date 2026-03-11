#include "rpi.h"
#include "mbox.h"
#include "cam-power.h"
#include "i2c.h"
#include "imx477.h"
#include "unicam.h"
#include "pi-sd.h"
#include "mbr.h"
#include "fat32.h"
#include "frame.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477,
    BSC0_BASE          = 0x20205000,
};

// SD card / FAT32 state (initialized by frame_sd_init).
static fat32_fs_t sd_fs;
static pi_dirent_t sd_root;
static int sd_ready = 0;

void frame_init(frame_t *f) {
    // Boost core clock for SDRAM bandwidth.
    rpi_clock_hz_set(MBOX_CLK_CORE, 400 * 1000 * 1000);
    output("Core clock: %d Hz\n", rpi_clock_curhz_get(MBOX_CLK_CORE));

    // Power on camera (GPIO 44 low->high sequence).
    cam_power_on();
    output("Camera powered on.\n");

    // Init I2C on BSC0 (GPIO 28/29).
    i2c_init_ex(BSC0_BASE, 28, 29);

    // Verify IMX477 chip ID.
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

    // Enable Unicam power domains + CAM1 clock.
    unicam_power_init();

    // Also enable UNICAM0 power domain (firmware ID 13).
    {
        volatile uint32_t msg[8] __attribute__((aligned(16)));
        msg[0] = 8 * 4; msg[1] = 0;
        msg[2] = 0x00038030; msg[3] = 8; msg[4] = 0;
        msg[5] = 13; msg[6] = 1; msg[7] = 0;
        mbox_send(MBOX_CH, msg);
    }

    // Full sensor init (common + mode tables, stays in standby).
    imx477_init();
    imx477_set_test_pattern(0);
    output("Sensor initialized.\n");

    // Allocate GPU memory for DMA buffer.
    f->gpu_handle = gpu_mem_alloc(FRAME_ALLOC_SIZE, 4096,
                        GPU_MEM_FLAG_DIRECT | GPU_MEM_FLAG_ZERO);
    if (!f->gpu_handle) panic("gpu_mem_alloc failed!\n");

    f->bus_addr = gpu_mem_lock(f->gpu_handle);
    if (!f->bus_addr) panic("gpu_mem_lock failed!\n");

    f->buf = (volatile uint8_t *)(f->bus_addr & 0x3FFFFFFF);
    f->result = 0;
    f->ibwp = 0;

    output("GPU buffer: handle=%x bus=%x (%d bytes)\n",
           f->gpu_handle, f->bus_addr, FRAME_ALLOC_SIZE);
}

int frame_capture(frame_t *f) {
    // Configure Unicam CSI-2 receiver.
    unicam_rx_init(f->bus_addr, FRAME_ALLOC_SIZE, FRAME_LINE_STRIDE);

    // Start sensor streaming, immediately wait for frame.
    imx477_start_streaming();
    f->result = unicam_wait_frame(1000000);

    // Read write pointer before stopping (clock gate reset clears it).
    dev_barrier();
    f->ibwp = GET32(UNICAM_BASE + UNICAM_IBWP);

    // Stop sensor and receiver.
    imx477_stop_streaming();
    unicam_stop();

    return f->result;
}

void frame_sd_init(void) {
    kmalloc_init();
    pi_sd_init();

    mbr_t *mbr = mbr_read();
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    sd_fs = fat32_mk(&partition);
    sd_root = fat32_get_root(&sd_fs);
    sd_ready = 1;

    output("SD card ready.\n");
}

int frame_save(frame_t *f, const char *filename) {
    if (!sd_ready)
        panic("frame_save: call frame_sd_init first!\n");

    // Delete old file if it exists.
    fat32_delete(&sd_fs, &sd_root, (char *)filename);

    // Create new file.
    pi_dirent_t *created = fat32_create(&sd_fs, &sd_root, (char *)filename, 0);
    if (!created) {
        output("WARNING: fat32_create(%s) failed!\n", filename);
        return 0;
    }

    // Write frame data.
    pi_file_t file = {
        .data = (char *)f->buf,
        .n_data = FRAME_BUF_SIZE,
        .n_alloc = FRAME_BUF_SIZE,
    };
    int wr = fat32_write(&sd_fs, &sd_root, (char *)filename, &file);
    return wr;
}

void frame_cleanup(frame_t *f) {
    gpu_mem_unlock(f->gpu_handle);
    gpu_mem_free(f->gpu_handle);
    f->gpu_handle = 0;
    f->bus_addr = 0;
    f->buf = 0;
}
