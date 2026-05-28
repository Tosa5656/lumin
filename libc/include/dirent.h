#ifndef _DIRENT_H
#define _DIRENT_H

#include <stdint.h>
#include <sys/types.h>

#define VFS_NAME_MAX 256

struct vfs_dentry {
    char     name[VFS_NAME_MAX];
    uint64_t ino;
    int      type;
    uint64_t size;
};

#endif
