#ifndef KERNEL_STAT_H
#define KERNEL_STAT_H

#include <stdint.h>

#define S_IFMT   0170000
#define S_IFREG  0100000
#define S_IFDIR  0040000

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    int      st_mode;
    uint64_t st_uid;
    uint64_t st_gid;
    uint64_t st_rdev;
    uint64_t st_size;
    uint64_t st_blksize;
    uint64_t st_blocks;
};

#endif
