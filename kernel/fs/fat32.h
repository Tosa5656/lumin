#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include "block/block.h"
#include "vfs.h"

struct __attribute__((packed)) fat32_bpb {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
};

#define FAT32_SIGNATURE 0xAA55
#define FAT32_EOC       0x0FFFFFF8
#define FAT32_BAD       0x0FFFFFF7
#define FAT32_FREE      0x00000000
#define FAT_MASK        0x0FFFFFFF

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LFN        0x0F

struct __attribute__((packed)) fat32_dent {
    char     name[11];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  ctime_tenth;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t cluster_hi;
    uint16_t wtime;
    uint16_t wdate;
    uint16_t cluster_lo;
    uint32_t size;
};

#define FAT32_MAX_LFN_ENTRIES 20

struct fat32_lfn_part {
    uint8_t  seq;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t reserved;
    uint16_t name3[2];
} __attribute__((packed));

struct fat32_fs {
    struct block_device *bdev;
    struct fat32_bpb     bpb;
    uint32_t             fat_start;
    uint32_t             data_start;
    uint32_t             sectors_per_fat;
    uint32_t             total_clusters;
};

int  fat32_mount(struct block_device *bdev, struct fat32_fs *fs);
int  fat32_read_cluster(struct fat32_fs *fs, uint32_t cluster, void *buf);
int  fat32_read_dir(struct fat32_fs *fs, uint32_t cluster, uint32_t index, struct vfs_dentry *entry);
int  fat32_lookup(struct fat32_fs *fs, uint32_t cluster, const char *name,
                  uint32_t *out_cluster, uint32_t *out_size, int *out_dir,
                  uint32_t *out_sector, int *out_offset);
int  fat32_read_file(struct fat32_fs *fs, uint32_t cluster, uint32_t size, uint64_t offset, uint64_t count, void *buf);

int  fat32_write_file(struct fat32_fs *fs, uint32_t *cluster, uint32_t *size, uint64_t offset, uint64_t count, const void *buf);
int  fat32_truncate(struct fat32_fs *fs, uint32_t *cluster, uint32_t *size, uint64_t new_size);
int  fat32_create_file(struct fat32_fs *fs, uint32_t dir_cluster, const char *name,
                       uint32_t *out_cluster, uint32_t *out_sector, int *out_offset);
int  fat32_unlink_file(struct fat32_fs *fs, uint32_t dir_cluster, const char *name);
int  fat32_mkdir(struct fat32_fs *fs, uint32_t dir_cluster, const char *name,
                 uint32_t *out_cluster, uint32_t *out_sector, int *out_offset);

int  vfs_mount_fat32(const char *path, struct block_device *bdev);

#endif