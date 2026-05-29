#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_NAME_MAX   256
#define VFS_MOUNT_MAX  16
#define VFS_FILE_MAX   64
#define VFS_PATH_MAX   512

enum vfs_inode_type {
    VFS_FILE,
    VFS_DIR,
    VFS_BLKDEV,
};

struct vfs_inode;
struct vfs_superblock;
struct vfs_dentry;
struct block_device;

struct vfs_inode_ops {
    int (*read)(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf);
    int (*write)(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf);
    int (*truncate)(struct vfs_inode *inode, uint64_t size);
    int (*readdir)(struct vfs_inode *dir, uint32_t index, struct vfs_dentry *entry);
    int (*lookup)(struct vfs_inode *dir, const char *name, struct vfs_inode **child);
    int (*create)(struct vfs_inode *dir, const char *name, struct vfs_inode **child);
    int (*unlink)(struct vfs_inode *dir, const char *name);
    int (*mkdir)(struct vfs_inode *dir, const char *name);
};

struct vfs_inode {
    uint64_t               ino;
    enum vfs_inode_type    type;
    uint64_t               size;
    struct vfs_superblock *sb;
    struct vfs_inode_ops  *ops;
    void                  *private;
    uint32_t               refcount;
};

struct vfs_dentry {
    char    name[VFS_NAME_MAX];
    uint64_t ino;
    enum vfs_inode_type type;
    uint64_t size;
};

struct vfs_sb_ops {
    struct vfs_inode *(*alloc_inode)(struct vfs_superblock *sb);
    void (*destroy_inode)(struct vfs_inode *inode);
    struct vfs_inode *(*get_root)(struct vfs_superblock *sb);
};

struct vfs_superblock {
    int                mounted;
    struct vfs_sb_ops *sb_ops;
    struct vfs_inode  *root;
    void              *private;
};

struct vfs_file {
    struct vfs_inode *inode;
    uint64_t          offset;
    int               flags;
};

#define VFS_O_READ  1
#define VFS_O_WRITE 2

int  vfs_init(void);
int  vfs_mount(const char *path, struct vfs_superblock *sb);
int  vfs_umount(const char *path);
int  vfs_mount_devfs(const char *path);
struct block_device *vfs_get_block_device(const char *path);

struct vfs_file *vfs_open(const char *path, int flags);
struct vfs_file *vfs_dup(struct vfs_file *file);
int  vfs_read(struct vfs_file *file, uint64_t size, void *buf);
int  vfs_write(struct vfs_file *file, uint64_t size, const void *buf);
int  vfs_close(struct vfs_file *file);
int  vfs_readdir(const char *path, uint32_t index, struct vfs_dentry *entry);
int  vfs_stat(const char *path, struct vfs_dentry *entry);
int  vfs_create(const char *path);
int  vfs_unlink(const char *path);
int  vfs_truncate(const char *path, uint64_t size);
int  vfs_mkdir(const char *path);

void vfs_inode_ref(struct vfs_inode *inode);
void vfs_inode_unref(struct vfs_inode *inode);

#endif
