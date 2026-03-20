#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"

// Print extra tracing info when this is enabled.  You can and should add your
// own.
static int trace_p = 0;
static int init_p = 0;

fat32_boot_sec_t boot_sector;

fat32_fs_t fat32_mk(mbr_partition_ent_t *partition) {
  demand(!init_p, "the fat32 module is already in use\n");
  // TODO: Read the boot sector (of the partition) off the SD card.
  boot_sector = *(fat32_boot_sec_t *) pi_sec_read(partition->lba_start, 1);

  // TODO: Verify the boot sector (also called the volume id, `fat32_volume_id_check`)
  fat32_volume_id_check(&boot_sector);
  fat32_volume_id_print("boot sector", &boot_sector);

  // TODO: Read the FS info sector (the sector immediately following the boot
  // sector) and check it (`fat32_fsinfo_check`, `fat32_fsinfo_print`)
  assert(boot_sector.info_sec_num == 1);
  struct fsinfo info = *(struct fsinfo *) pi_sec_read(partition->lba_start + 1, 1);
  fat32_fsinfo_check(&info);
  fat32_fsinfo_print("info struct", &info);
  // END OF PART 2
  // The rest of this is for Part 3:

  // TODO: calculate the fat32_fs_t metadata, which we'll need to return.
  unsigned lba_start = partition->lba_start; // from the partition
  unsigned fat_begin_lba = lba_start + boot_sector.reserved_area_nsec; // the start LBA + the number of reserved sectors
  unsigned cluster_begin_lba = fat_begin_lba + (boot_sector.nfats * boot_sector.nsec_per_fat); // the beginning of the FAT, plus the combined length of all the FATs
  unsigned sec_per_cluster = boot_sector.sec_per_cluster; // from the boot sector
  unsigned root_first_cluster = boot_sector.first_cluster; // from the boot sector
  unsigned n_entries = boot_sector.nsec_in_fs / boot_sector.sec_per_cluster; // from the boot sector
  //unimplemented();

  /*
   * TODO: Read in the entire fat (one copy: worth reading in the second and
   * comparing).
   *
   * The disk is divided into clusters. The number of sectors per
   * cluster is given in the boot sector byte 13. <sec_per_cluster>
   *
   * The File Allocation Table has one entry per cluster. This entry
   * uses 12, 16 or 28 bits for FAT12, FAT16 and FAT32.
   *
   * Store the FAT in a heap-allocated array.
   */
  uint32_t *fat;
  fat = (uint32_t *) pi_sec_read(fat_begin_lba, boot_sector.nsec_per_fat);

  /** check that fats are the same; warn + sync if not */
  // Skipped for speed — we only write FAT1 now.
  // uint32_t *fat2 = (uint32_t *) pi_sec_read(fat_begin_lba + boot_sector.nsec_per_fat, boot_sector.nsec_per_fat);
  // int fats_differ = 0;
  // for (int i = 0; i < boot_sector.nsec_per_fat * (512 / sizeof(uint32_t)); i++) {
  //   if (fat[i] != fat2[i]) { fats_differ = 1; break; }
  // }
  // if (fats_differ) {
  //   trace("WARNING: FATs differ, syncing FAT2 from FAT1\n");
  //   pi_sd_write(fat, fat_begin_lba + boot_sector.nsec_per_fat, boot_sector.nsec_per_fat);
  // }

  // Create the FAT32 FS struct with all the metadata
  fat32_fs_t fs = (fat32_fs_t) {
    .lba_start = lba_start,
      .fat_begin_lba = fat_begin_lba,
      .cluster_begin_lba = cluster_begin_lba,
      .sectors_per_cluster = sec_per_cluster,
      .root_dir_first_cluster = root_first_cluster,
      .fat = fat,
      .n_entries = n_entries,
  };

  if (trace_p) {
    trace("begin lba = %d\n", fs.fat_begin_lba);
    trace("cluster begin lba = %d\n", fs.cluster_begin_lba);
    trace("sectors per cluster = %d\n", fs.sectors_per_cluster);
    trace("root dir first cluster = %d\n", fs.root_dir_first_cluster);
  }

  init_p = 1;
  return fs;
}

// Given cluster_number, get lba.  Helper function.
static uint32_t cluster_to_lba(fat32_fs_t *f, uint32_t cluster_num) {
  assert(cluster_num >= 2);
  // TODO: calculate LBA from cluster number, cluster_begin_lba, and
  // sectors_per_cluster
  unsigned lba;
  lba = f->cluster_begin_lba + (cluster_num - 2) * f->sectors_per_cluster;
  if (trace_p) trace("cluster %d to lba: %d\n", cluster_num, lba);
  return lba;
}

pi_dirent_t fat32_get_root(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // TODO: return the information corresponding to the root directory (just
  // cluster_id, in this case)
  
  return (pi_dirent_t) {
    .name = "",
      .raw_name = "",
      .cluster_id = fs->root_dir_first_cluster, // fix this
      .is_dir_p = 1,
      .nbytes = 0,
  };
}

// Given the starting cluster index, get the length of the chain.  Helper
// function.
static uint32_t get_cluster_chain_length(fat32_fs_t *fs, uint32_t start_cluster) {
  uint32_t next_cluster = start_cluster;
  uint32_t cluster_chain_length = 0;
  // trace("start cluster = %d\n", start_cluster);
  while (fat32_fat_entry_type(next_cluster) != LAST_CLUSTER) {
    next_cluster = fs->fat[next_cluster];
    cluster_chain_length++;
    // trace("next cluster = %d\n", next_cluster);
  }
  return cluster_chain_length;

}

// Given the starting cluster index, read a cluster chain into a contiguous
// buffer.  Assume the provided buffer is large enough for the whole chain.
// Helper function.
static void read_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data) {
  uint32_t next_cluster = start_cluster;
  uint32_t cluster_size = fs->sectors_per_cluster * boot_sector.bytes_per_sec;
  while (fat32_fat_entry_type(next_cluster) != LAST_CLUSTER) {
    uint32_t lba = cluster_to_lba(fs, next_cluster);
    uint8_t *cluster_data = (uint8_t *) pi_sec_read(lba, fs->sectors_per_cluster);
    memcpy(data, cluster_data, cluster_size);
    data += cluster_size;
    next_cluster = fs->fat[next_cluster];
  }
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // fat32_fat_entry_type(cluster) == LAST_CLUSTER.  For each cluster, copy it
  // to the buffer (`data`).  Be sure to offset your data pointer by the
  // appropriate amount each time.

}

// Converts a fat32 internal dirent into a generic one suitable for use outside
// this driver.
static pi_dirent_t dirent_convert(fat32_dirent_t *d) {
  pi_dirent_t e = {
    .cluster_id = fat32_cluster_id(d),
    .is_dir_p = d->attr == FAT32_DIR,
    .nbytes = d->file_nbytes,
  };
  // can compare this name
  memcpy(e.raw_name, d->filename, sizeof d->filename);
  // for printing.
  fat32_dirent_name(d,e.name);
  return e;
}

// Gets all the dirents of a directory which starts at cluster `cluster_start`.
// Return a heap-allocated array of dirents.
static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n) {
  // TODO: figure out the length of the cluster chain (see
  // `get_cluster_chain_length`)
  uint32_t cluster_chain_length = get_cluster_chain_length(fs, cluster_start);
  // printk("cluster chain length: %d\n", cluster_chain_length);
  // TODO: allocate a buffer large enough to hold the whole directory
  uint8_t *buffer = kmalloc(cluster_chain_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec);
  // TODO: read in the whole directory (see `read_cluster_chain`)
  // trace("sectors per cluster: %d\n", fs->sectors_per_cluster);
  // trace("bytes per sec: %d\n", boot_sector.bytes_per_sec);
  // trace("size of dirent: %d\n", sizeof(fat32_dirent_t));
  read_cluster_chain(fs, cluster_start, buffer);
  *dir_n = cluster_chain_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec / sizeof(fat32_dirent_t);
  return (fat32_dirent_t *)buffer;
}

pi_directory_t fat32_readdir(fat32_fs_t *fs, pi_dirent_t *dirent) {
  demand(init_p, "fat32 not initialized!");
  demand(dirent->is_dir_p, "tried to readdir a file!");
  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, dirent->cluster_id, &n_dirents);
  // trace("n_dirents: %d\n", n_dirents);
  // TODO: allocate space to store the pi_dirent_t return values
  pi_dirent_t *pi_dirents = kmalloc(n_dirents * sizeof(pi_dirent_t));

  // TODO: iterate over the directory and create pi_dirent_ts for every valid
  // file.  Don't include empty dirents, LFNs, or Volume IDs.  You can use
  // `dirent_convert`.
  uint32_t num_valid_dirents = 0;
  for (int i = 0; i < n_dirents; i++) {
    if (fat32_dirent_free(&dirents[i])) continue; // free space
    if (fat32_dirent_is_lfn(&dirents[i])) continue; // LFN version of name
    if (dirents[i].attr & FAT32_VOLUME_LABEL) continue; // volume label
    pi_dirents[num_valid_dirents] = dirent_convert(&dirents[i]);
    num_valid_dirents++;
  }

  // TODO: create a pi_directory_t using the dirents and the number of valid
  // dirents we found
  return (pi_directory_t) {
    .dirents = pi_dirents,
    .ndirents = num_valid_dirents,
  };
}

static int find_dirent_with_name(fat32_dirent_t *dirents, int n, char *filename) {
  // TODO: iterate through the dirents, looking for a file which matches the
  // name; use `fat32_dirent_name` to convert the internal name format to a
  // normal string.
  for (int i = 0; i < n; i++) {
    char name[1024];
    if (fat32_dirent_free(&dirents[i])) continue; // free space
    if (fat32_dirent_is_lfn(&dirents[i])) continue; // LFN version of name
    if (dirents[i].attr & FAT32_VOLUME_LABEL) continue; // volume label
    fat32_dirent_name(&dirents[i], name);
    if (strcmp(name, filename) == 0) return i;
  }
  return -1;
}

pi_dirent_t *fat32_stat(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory");

  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);

  // TODO: Iterate through the directory's entries and find a dirent with the
  // provided name.  Return NULL if no such dirent exists.  You can use
  // `find_dirent_with_name` if you've implemented it.
  uint32_t index = find_dirent_with_name(dirents, n_dirents, filename);
  if (index == -1) return NULL;

  // TODO: allocate enough space for the dirent, then convert
  // (`dirent_convert`) the fat32 dirent into a Pi dirent.
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(&dirents[index]);
  return dirent;
}

pi_file_t *fat32_read(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // This should be pretty similar to readdir, but simpler.
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: read the dirents of the provided directory and look for one matching the provided name
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  uint32_t index = find_dirent_with_name(dirents, n_dirents, filename);
  if (index == -1) return NULL;

  // TODO: figure out the length of the cluster chain
  uint32_t cluster_chain_length = get_cluster_chain_length(fs, fat32_cluster_id(&dirents[index]));

  // TODO: allocate a buffer large enough to hold the whole file
  uint8_t *buffer = kmalloc(cluster_chain_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec);

  // TODO: read in the whole file (if it's not empty)
  if (dirents[index].file_nbytes > 0) {
    read_cluster_chain(fs, fat32_cluster_id(&dirents[index]), buffer);
  }

  // TODO: fill the pi_file_t
  pi_file_t *file = kmalloc(sizeof(pi_file_t));
  *file = (pi_file_t) {
    .data = buffer,
    .n_data = dirents[index].file_nbytes,
    .n_alloc = cluster_chain_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec,
  };
  return file;
}

/******************************************************************************
 * Everything below here is for writing to the SD card (Part 7/Extension).  If
 * you're working on read-only code, you don't need any of this.
 ******************************************************************************/

static uint32_t find_free_cluster(fat32_fs_t *fs, uint32_t start_cluster) {
  if (start_cluster < 3) start_cluster = 3;
  for (uint32_t i = start_cluster; i < fs->n_entries; i++) {
    if (fat32_fat_entry_type(fs->fat[i]) == FREE_CLUSTER)
      return i;
  }
  panic("No more clusters on the disk!\n");
}

// to improve speed of jpeg writes, we track which sectors are dirty
static uint32_t fat_dirty_lo = 0xFFFFFFFF;  // lowest dirty sector
static uint32_t fat_dirty_hi = 0;            // highest dirty sector + 1

// macro for tracking dirty sectors
#define FAT_SET(fs, cluster, val) do { \
  (fs)->fat[(cluster)] = (val); \
  uint32_t _sec = (cluster) / 128; \
  if (_sec < fat_dirty_lo) fat_dirty_lo = _sec; \
  if (_sec + 1 > fat_dirty_hi) fat_dirty_hi = _sec + 1; \
} while(0)

static void write_fat_to_disk(fat32_fs_t *fs) {
  if (trace_p) trace("syncing FAT\n");
  if (fat_dirty_lo >= fat_dirty_hi) return;  // nothing dirty
  uint32_t nsec = fat_dirty_hi - fat_dirty_lo;
  uint8_t *base = (uint8_t *)fs->fat + fat_dirty_lo * 512;

  if (fat_dirty_hi > boot_sector.nsec_per_fat)
    fat_dirty_hi = boot_sector.nsec_per_fat;
  if (fat_dirty_lo >= fat_dirty_hi) { fat_dirty_lo = 0xFFFFFFFF; fat_dirty_hi = 0; return; }
  nsec = fat_dirty_hi - fat_dirty_lo;
  base = (uint8_t *)fs->fat + fat_dirty_lo * 512;

  // write all dirty sectors to disk
  pi_sd_write(base, fs->fat_begin_lba + fat_dirty_lo, nsec);
  
  // once write is done, no sectors are dirty
  fat_dirty_lo = 0xFFFFFFFF;
  fat_dirty_hi = 0;
}

static void write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) {
  uint32_t cluster_size = fs->sectors_per_cluster * boot_sector.bytes_per_sec;
  uint32_t cur = start_cluster;
  uint32_t prev = 0;
  uint32_t written = 0;

  if (nbytes == 0) {    // mark every cluster as free
    if (fat32_fat_entry_type(cur) == USED_CLUSTER ||
        fat32_fat_entry_type(cur) == LAST_CLUSTER) {
      uint32_t c = cur;
      while (fat32_fat_entry_type(c) == USED_CLUSTER) {
        uint32_t next = fs->fat[c];
        FAT_SET(fs, c, FREE_CLUSTER);
        c = next;
      }
      if (fat32_fat_entry_type(c) == LAST_CLUSTER)
        FAT_SET(fs, c, FREE_CLUSTER);
    }
    return;
  }

  while (written < nbytes &&
         (fat32_fat_entry_type(cur) == USED_CLUSTER ||
          fat32_fat_entry_type(cur) == LAST_CLUSTER)) {
    uint32_t to_write = nbytes - written;
    if (to_write >= cluster_size) {
      pi_sd_write(data + written, cluster_to_lba(fs, cur), fs->sectors_per_cluster);
    } else {
      uint8_t tmp[cluster_size];
      memset(tmp, 0, cluster_size);
      memcpy(tmp, data + written, to_write);
      pi_sd_write(tmp, cluster_to_lba(fs, cur), fs->sectors_per_cluster);
    }
    written += cluster_size;
    prev = cur;
    if (fat32_fat_entry_type(fs->fat[cur]) == LAST_CLUSTER)
      break;
    cur = fs->fat[cur];
  }

  while (written < nbytes) {
    uint32_t new_cluster = find_free_cluster(fs, 3);
    if (prev)
      FAT_SET(fs, prev, new_cluster);   // add entry to FAT
    FAT_SET(fs, new_cluster, 0x0FFFFFFF); // LAST_CLUSTER

    uint32_t to_write = nbytes - written;
    if (to_write >= cluster_size) {
      pi_sd_write(data + written, cluster_to_lba(fs, new_cluster), fs->sectors_per_cluster);
    } else {
      uint8_t tmp[cluster_size];
      memset(tmp, 0, cluster_size);
      memcpy(tmp, data + written, to_write);
      pi_sd_write(tmp, cluster_to_lba(fs, new_cluster), fs->sectors_per_cluster);
    }
    written += cluster_size;
    prev = new_cluster;
  }

  // free clusters that are leftover
  if (prev && fat32_fat_entry_type(fs->fat[prev]) != LAST_CLUSTER &&
      fat32_fat_entry_type(fs->fat[prev]) == USED_CLUSTER) {
    uint32_t c = fs->fat[prev];
    FAT_SET(fs, prev, 0x0FFFFFFF);        // LAST_CLUSTER
    while (fat32_fat_entry_type(c) == USED_CLUSTER) {
      uint32_t next = fs->fat[c];
      FAT_SET(fs, c, FREE_CLUSTER);  // set cluster as free
      c = next;
    }
    if (fat32_fat_entry_type(c) == LAST_CLUSTER)
      FAT_SET(fs, c, FREE_CLUSTER);
  } else if (prev) {
    FAT_SET(fs, prev, 0x0FFFFFFF);
  }
}

int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("renaming %s to %s\n", oldname, newname);
  if (!fat32_is_valid_name(newname)) return 0;

  uint32_t n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n);

  if (find_dirent_with_name(dirents, n, newname) >= 0) return 0;

  int idx = find_dirent_with_name(dirents, n, oldname);
  if (idx < 0) return 0;

  fat32_dirent_set_name(&dirents[idx], newname);

  uint32_t dir_bytes = n * sizeof(fat32_dirent_t);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_bytes);
  write_fat_to_disk(fs);
  return 1;
}

pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) return NULL;

  uint32_t n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n);

  // check for duplicates
  if (find_dirent_with_name(dirents, n, filename) >= 0) {
    if (trace_p) trace("file %s already exists\n", filename);
    return NULL;
  }

  // find a free entry in the directory
  int free_idx = -1;
  for (uint32_t i = 0; i < n; i++) {
    if (fat32_dirent_free(&dirents[i])) {
      free_idx = i;
      break;
    }
  }
  if (free_idx < 0)
    panic("No free directory entries!\n");

  // create the new entry
  memset(&dirents[free_idx], 0, sizeof(fat32_dirent_t));
  fat32_dirent_set_name(&dirents[free_idx], filename);
  dirents[free_idx].attr = is_dir ? FAT32_DIR : FAT32_ARCHIVE;
  dirents[free_idx].hi_start = 0;
  dirents[free_idx].lo_start = 0;
  dirents[free_idx].file_nbytes = 0;

  uint32_t dir_bytes = n * sizeof(fat32_dirent_t);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_bytes);
  write_fat_to_disk(fs);

  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(&dirents[free_idx]);
  return dirent;
}
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("deleting %s\n", filename);
  if (!fat32_is_valid_name(filename)) return 0;

  uint32_t n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n);

  int idx = find_dirent_with_name(dirents, n, filename);
  if (idx < 0) {
    if (trace_p) trace("file %s not found for deletion\n", filename);
    return 0;
  }

  // mark as deleted
  dirents[idx].filename[0] = 0xE5;

  // free cluster chain
  uint32_t clust = fat32_cluster_id(&dirents[idx]);
  if (clust >= 2) {
    while (fat32_fat_entry_type(clust) == USED_CLUSTER) {
      uint32_t next = fs->fat[clust];
      FAT_SET(fs, clust, FREE_CLUSTER);
      clust = next;
    }
    if (fat32_fat_entry_type(clust) == LAST_CLUSTER)
      FAT_SET(fs, clust, FREE_CLUSTER);
  }

  uint32_t dir_bytes = n * sizeof(fat32_dirent_t);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_bytes);
  write_fat_to_disk(fs);
  return 1;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("truncating %s\n", filename);

  uint32_t n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n);
  int idx = find_dirent_with_name(dirents, n, filename);
  if (idx < 0) return 0;

  uint32_t clust = fat32_cluster_id(&dirents[idx]);

  if (length == 0) {
    if (clust >= 2) {
      while (fat32_fat_entry_type(clust) == USED_CLUSTER) {
        uint32_t next = fs->fat[clust];
        FAT_SET(fs, clust, FREE_CLUSTER);
        clust = next;
      }
      if (fat32_fat_entry_type(clust) == LAST_CLUSTER)
        FAT_SET(fs, clust, FREE_CLUSTER);
    }
    dirents[idx].hi_start = 0;
    dirents[idx].lo_start = 0;
  } else if (clust < 2) {
    uint32_t new_clust = find_free_cluster(fs, 3);
    FAT_SET(fs, new_clust, 0x0FFFFFFF);
    dirents[idx].hi_start = new_clust >> 16;
    dirents[idx].lo_start = new_clust & 0xFFFF;
  }

  dirents[idx].file_nbytes = length;

  uint32_t dir_bytes = n * sizeof(fat32_dirent_t);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_bytes);
  write_fat_to_disk(fs);
  return 1;
}

int fat32_write(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, pi_file_t *file) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  uint32_t n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n);
  int idx = find_dirent_with_name(dirents, n, filename);
  if (idx < 0) return 0;

  uint32_t clust = fat32_cluster_id(&dirents[idx]);

  // if file has no cluster chain, create one
  if (clust < 2 && file->n_data > 0) {
    clust = find_free_cluster(fs, 3);
    FAT_SET(fs, clust, 0x0FFFFFFF); // LAST_CLUSTER
    dirents[idx].hi_start = clust >> 16;
    dirents[idx].lo_start = clust & 0xFFFF;
  }

  dirents[idx].file_nbytes = file->n_data;

  if (file->n_data > 0)
    write_cluster_chain(fs, clust, (uint8_t *)file->data, file->n_data);

  uint32_t dir_bytes = n * sizeof(fat32_dirent_t);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_bytes);

  write_fat_to_disk(fs);
  return 1;
}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}
