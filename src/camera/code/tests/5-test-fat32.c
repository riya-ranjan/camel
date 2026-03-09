// Test FAT32 write support: init SD, list directory, create/write/read a file.
#include "rpi.h"
#include "pi-sd.h"
#include "mbr.h"
#include "fat32.h"

void notmain(void) {
    output("--- FAT32 Write Test ---\n");

    // 1. Init heap (needed by FAT32 for kmalloc).
    kmalloc_init();
    output("Heap initialized.\n");

    // 2. Init SD card.
    pi_sd_init();
    output("SD card initialized.\n");

    // 3. Read MBR + mount FAT32.
    mbr_t *mbr = mbr_read();
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    fat32_fs_t fs = fat32_mk(&partition);
    pi_dirent_t root = fat32_get_root(&fs);
    output("FAT32 mounted.\n");

    // 4. List root directory contents.
    output("\n=== Root directory BEFORE write ===\n");
    pi_directory_t dir = fat32_readdir(&fs, &root);
    for (unsigned i = 0; i < dir.ndirents; i++) {
        output("  %s  %s  %d bytes  cluster=%d\n",
            dir.dirents[i].name,
            dir.dirents[i].is_dir_p ? "[DIR]" : "[FILE]",
            dir.dirents[i].nbytes,
            dir.dirents[i].cluster_id);
    }

    // 5. Delete TEST.TXT if it exists (clean slate).
    output("\nDeleting TEST.TXT (if exists)...\n");
    fat32_delete(&fs, &root, "TEST.TXT");

    // 6. Create TEST.TXT.
    output("Creating TEST.TXT...\n");
    pi_dirent_t *created = fat32_create(&fs, &root, "TEST.TXT", 0);
    if (!created) {
        output("ERROR: fat32_create failed!\n");
        clean_reboot();
    }
    output("Created: %s cluster=%d\n", created->name, created->cluster_id);

    // 7. Write test data.
    char test_data[] = "Hello from bare-metal Pi! FAT32 write works!\n";
    pi_file_t file = {
        .data = test_data,
        .n_data = sizeof(test_data) - 1,  // exclude null terminator
        .n_alloc = sizeof(test_data),
    };
    output("Writing %d bytes to TEST.TXT...\n", file.n_data);
    int wr = fat32_write(&fs, &root, "TEST.TXT", &file);
    if (!wr) {
        output("ERROR: fat32_write failed!\n");
        clean_reboot();
    }
    output("Write succeeded.\n");

    // 8. Read it back and verify.
    output("Reading TEST.TXT back...\n");
    pi_file_t *readback = fat32_read(&fs, &root, "TEST.TXT");
    if (!readback) {
        output("ERROR: fat32_read failed!\n");
        clean_reboot();
    }
    output("Read %d bytes: \"", readback->n_data);
    for (unsigned i = 0; i < readback->n_data; i++)
        output("%c", readback->data[i]);
    output("\"\n");

    // Verify contents match.
    if (readback->n_data == file.n_data &&
        memcmp(readback->data, file.data, file.n_data) == 0) {
        output("VERIFY: contents match!\n");
    } else {
        output("VERIFY FAILED: contents differ!\n");
        output("  expected %d bytes, got %d bytes\n", file.n_data, readback->n_data);
    }

    // 9. List directory again to confirm file appears.
    output("\n=== Root directory AFTER write ===\n");
    dir = fat32_readdir(&fs, &root);
    for (unsigned i = 0; i < dir.ndirents; i++) {
        output("  %s  %s  %d bytes  cluster=%d\n",
            dir.dirents[i].name,
            dir.dirents[i].is_dir_p ? "[DIR]" : "[FILE]",
            dir.dirents[i].nbytes,
            dir.dirents[i].cluster_id);
    }

    output("\n--- FAT32 Write Test: DONE ---\n");
    clean_reboot();
}
