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

typedef struct DIR {
    char *path;
    int pos;
    struct vfs_dentry current;
    int has_next;
} DIR;

DIR *opendir(const char *path);
struct vfs_dentry *readdir(DIR *dir);
int closedir(DIR *dir);

#endif
