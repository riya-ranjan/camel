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
#include "jpeg.h"

enum {
    IMX477_I2C_ADDR    = 0x1A,
    IMX477_CHIP_ID_REG = 0x0016,
    IMX477_EXPECTED_ID = 0x0477,
};

// SD card / FAT32 state
static fat32_fs_t sd_fs;
static pi_dirent_t sd_root;
static int sd_ready = 0;

void frame_init(frame_t *f) {
    // boost core clock for faster data transfer (400 MHz)
    rpi_clock_hz_set(MBOX_CLK_CORE, 400 * 1000 * 1000);
    output("Core clock: %d Hz\n", rpi_clock_curhz_get(MBOX_CLK_CORE));

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

    unicam_power_init();

    imx477_init();
    output("Sensor initialized.\n");

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
    unicam_rx_init(f->bus_addr, FRAME_ALLOC_SIZE, FRAME_LINE_STRIDE);

    imx477_start_streaming();
    f->result = unicam_wait_frame(1000000);

    dev_barrier();
    f->ibwp = GET32(UNICAM_BASE + UNICAM_IBWP);

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

    fat32_delete(&sd_fs, &sd_root, (char *)filename);

    pi_dirent_t *created = fat32_create(&sd_fs, &sd_root, (char *)filename, 0);
    if (!created) {
        output("WARNING: fat32_create(%s) failed!\n", filename);
        return 0;
    }

    pi_file_t file = {
        .data = (char *)f->buf,
        .n_data = FRAME_BUF_SIZE,
        .n_alloc = FRAME_BUF_SIZE,
    };
    int wr = fat32_write(&sd_fs, &sd_root, (char *)filename, &file);
    return wr;
}

static uint8_t *jpeg_out_buf;

int frame_save_jpeg_rgb(const uint8_t *rgb, int width, int height,
                        const char *filename, int quality)
{
    if (!sd_ready)
        panic("frame_save_jpeg_rgb: call frame_sd_init first!\n");

    if (!jpeg_out_buf) {
        jpeg_out_buf = kmalloc(JPEG_OUT_SIZE);
        if (!jpeg_out_buf) panic("frame_save_jpeg_rgb: kmalloc failed!\n");
    }

    uint32_t t0 = timer_get_usec();
    int jpeg_size = jpeg_encode(rgb, width, height,
                                quality, jpeg_out_buf, JPEG_OUT_SIZE);
    uint32_t t1 = timer_get_usec();
    output("  JPEG encode: %d us (%d bytes)\n", t1 - t0, jpeg_size);

    if (jpeg_size == 0) {
        output("WARNING: JPEG encode failed!\n");
        return 0;
    }

    t0 = timer_get_usec();
    fat32_delete(&sd_fs, &sd_root, (char *)filename);
    pi_dirent_t *created = fat32_create(&sd_fs, &sd_root, (char *)filename, 0);
    if (!created) {
        output("WARNING: fat32_create(%s) failed!\n", filename);
        return 0;
    }

    pi_file_t file = {
        .data = (char *)jpeg_out_buf,
        .n_data = jpeg_size,
        .n_alloc = jpeg_size,
    };
    int wr = fat32_write(&sd_fs, &sd_root, (char *)filename, &file);
    t1 = timer_get_usec();
    output("  SD write: %d us\n", t1 - t0);

    return wr;
}

void frame_cleanup(frame_t *f) {
    gpu_mem_unlock(f->gpu_handle);
    gpu_mem_free(f->gpu_handle);
    f->gpu_handle = 0;
    f->bus_addr = 0;
    f->buf = 0;
}
