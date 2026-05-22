#include "vfs.h"
#include "../drivers/serial/serial.h"
#include "../block/block.h"
#include "../mm/kmalloc.h"
#include <stddef.h>

static struct vfs_file file_table[VFS_FILE_MAX];
static int file_count;

static struct vfs_superblock mounts[VFS_MOUNT_MAX];
static int mount_count;

void vfs_inode_ref(struct vfs_inode *inode)
{
    if (inode) inode->refcount++;
}

void vfs_inode_unref(struct vfs_inode *inode)
{
    if (!inode || inode->refcount == 0) return;
    inode->refcount--;
    if (inode->refcount == 0 && inode->sb && inode->sb->sb_ops && inode->sb->sb_ops->destroy_inode)
        inode->sb->sb_ops->destroy_inode(inode);
}

static int resolve_full(const char *path, struct vfs_inode **out_inode)
{
    if (!path || *path != '/' || !out_inode)
        return -1;
    if (mount_count == 0)
        return -1;

    struct vfs_inode *cur = mounts[0].root;
    if (!cur) return -1;
    vfs_inode_ref(cur);

    char buf[VFS_PATH_MAX];
    int i = 0, j;

    while (path[i] != '\0')
    {
        while (path[i] == '/') i++;
        if (path[i] == '\0') break;

        j = 0;
        while (path[i] != '/' && path[i] != '\0' && j < VFS_NAME_MAX - 1)
            buf[j++] = path[i++];
        buf[j] = '\0';

        if (cur->type != VFS_DIR || !cur->ops || !cur->ops->lookup)
        {
            vfs_inode_unref(cur);
            return -1;
        }

        struct vfs_inode *next = NULL;
        if (cur->ops->lookup(cur, buf, &next) != 0 || !next)
        {
            vfs_inode_unref(cur);
            return -1;
        }
        vfs_inode_unref(cur);
        cur = next;
    }

    *out_inode = cur;
    return 0;
}

int vfs_init(void)
{
    file_count = 0;
    mount_count = 0;
    for (int i = 0; i < VFS_FILE_MAX; i++)
        file_table[i].inode = NULL;
    serial_write("vfs: initialized\n");
    return 0;
}

int vfs_mount(const char *path, struct vfs_superblock *sb)
{
    if (!path || !sb || mount_count >= VFS_MOUNT_MAX)
        return -1;
    if (!sb->root || !sb->sb_ops)
        return -1;

    sb->mounted = 1;
    mounts[mount_count++] = *sb;
    serial_printf("vfs: mounted '%s'\n", path);
    return 0;
}

struct vfs_file *vfs_open(const char *path, int flags)
{
    if (!path || file_count >= VFS_FILE_MAX)
        return NULL;

    struct vfs_inode *inode = NULL;
    if (resolve_full(path, &inode) != 0)
        return NULL;

    struct vfs_file *f = &file_table[file_count++];
    f->inode = inode;
    f->offset = 0;
    f->flags = flags;
    return f;
}

int vfs_read(struct vfs_file *file, uint64_t size, void *buf)
{
    if (!file || !file->inode || !buf)
        return -1;
    if (!(file->flags & VFS_O_READ))
        return -1;
    if (!file->inode->ops || !file->inode->ops->read)
        return -1;

    int r = file->inode->ops->read(file->inode, file->offset, size, buf);
    if (r > 0)
        file->offset += r;
    return r;
}

int vfs_write(struct vfs_file *file, uint64_t size, const void *buf)
{
    if (!file || !file->inode || !buf)
        return -1;
    if (!(file->flags & VFS_O_WRITE))
        return -1;
    if (!file->inode->ops || !file->inode->ops->write)
        return -1;

    int r = file->inode->ops->write(file->inode, file->offset, size, buf);
    if (r > 0)
        file->offset += r;
    return r;
}

int vfs_close(struct vfs_file *file)
{
    if (!file || !file->inode)
        return -1;
    vfs_inode_unref(file->inode);
    file->inode = NULL;
    file->offset = 0;
    file->flags = 0;
    return 0;
}

int vfs_readdir(const char *path, uint32_t index, struct vfs_dentry *entry)
{
    if (!path || !entry)
        return -1;

    struct vfs_inode *inode = NULL;
    if (resolve_full(path, &inode) != 0)
        return -1;
    if (inode->type != VFS_DIR || !inode->ops || !inode->ops->readdir)
    {
        vfs_inode_unref(inode);
        return -1;
    }

    int r = inode->ops->readdir(inode, index, entry);
    vfs_inode_unref(inode);
    return r;
}

int vfs_stat(const char *path, struct vfs_dentry *entry)
{
    if (!path || !entry)
        return -1;

    struct vfs_inode *inode = NULL;
    if (resolve_full(path, &inode) != 0)
        return -1;

    entry->ino  = inode->ino;
    entry->type = inode->type;
    entry->size = inode->size;
    vfs_inode_unref(inode);
    return 0;
}

/* ───── devfs ───── */

struct devfs_entry {
    char    name[VFS_NAME_MAX];
    uint64_t ino;
    enum vfs_inode_type type;
    uint64_t size;
    void    *private;
};

#define DEVFS_MAX_ENTRIES 32

struct devfs_data {
    struct devfs_entry entries[DEVFS_MAX_ENTRIES];
    int entry_count;
    int next_ino;
};

static struct devfs_data devfs_data;
static struct vfs_inode  devfs_root_inode;
static struct vfs_superblock devfs_sb;

static struct vfs_inode *devfs_alloc_inode(struct vfs_superblock *sb)
{
    return (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
}

static void devfs_destroy_inode(struct vfs_inode *inode)
{
    if (inode) kfree(inode);
}

static int devfs_dir_readdir(struct vfs_inode *dir, uint32_t index, struct vfs_dentry *entry)
{
    struct devfs_data *d = (struct devfs_data *)dir->private;
    if (!d || index >= (uint32_t)d->entry_count)
        return -1;

    int n;
    for (n = 0; d->entries[index].name[n] && n < VFS_NAME_MAX - 1; n++)
        entry->name[n] = d->entries[index].name[n];
    entry->name[n] = '\0';
    entry->ino  = d->entries[index].ino;
    entry->type = d->entries[index].type;
    entry->size = d->entries[index].size;
    return 0;
}

static int devfs_blk_read(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf);
static int devfs_blk_write(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf);

static int devfs_dir_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **child)
{
    struct devfs_data *d = (struct devfs_data *)dir->private;
    if (!d || !name || !child)
        return -1;

    for (int i = 0; i < d->entry_count; i++)
    {
        int match = 1;
        const char *a = d->entries[i].name;
        const char *b = name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a != *b) match = 0;

        if (match)
        {
            struct vfs_inode *inode = devfs_alloc_inode(dir->sb);
            if (!inode) return -1;

            inode->ino     = d->entries[i].ino;
            inode->type    = d->entries[i].type;
            inode->size    = d->entries[i].size;
            inode->sb      = dir->sb;
            inode->private = d->entries[i].private;
            inode->refcount = 1;

            if (inode->type == VFS_DIR)
            {
                inode->ops = dir->ops;
                inode->private = d;
            }
            else if (inode->type == VFS_BLKDEV)
            {
                static struct vfs_inode_ops blk_ops = {
                    .read  = devfs_blk_read,
                    .write = devfs_blk_write,
                };
                inode->ops = &blk_ops;
            }

            *child = inode;
            return 0;
        }
    }
    return -1;
}

static struct vfs_inode_ops devfs_dir_ops = {
    .readdir = devfs_dir_readdir,
    .lookup  = devfs_dir_lookup,
};

static int devfs_blk_read(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    struct block_device *bdev = (struct block_device *)inode->private;
    if (!bdev) return -1;

    uint64_t sector = offset / bdev->sector_size;
    uint64_t byte_off = offset % bdev->sector_size;
    uint64_t to_read = size;
    uint8_t tmp[512];
    uint64_t done = 0;

    if (sector >= bdev->sector_count) return -1;
    if (to_read > bdev->sector_count * bdev->sector_size - offset)
        to_read = bdev->sector_count * bdev->sector_size - offset;

    if (byte_off)
    {
        if (block_read(bdev, sector, 1, tmp) != 0) return -1;
        uint64_t chunk = bdev->sector_size - byte_off;
        if (chunk > to_read) chunk = to_read;
        for (uint64_t i = 0; i < chunk; i++)
            ((uint8_t *)buf)[done++] = tmp[byte_off + i];
        sector++;
        to_read -= chunk;
    }

    while (to_read >= bdev->sector_size)
    {
        uint8_t cnt = 1;
        if (block_read(bdev, sector, cnt, &((uint8_t *)buf)[done]) != 0) return (int)done;
        done += bdev->sector_size;
        sector++;
        to_read -= bdev->sector_size;
    }

    if (to_read)
    {
        if (block_read(bdev, sector, 1, tmp) != 0) return (int)done;
        for (uint64_t i = 0; i < to_read; i++)
            ((uint8_t *)buf)[done++] = tmp[i];
    }

    return (int)done;
}

static int devfs_blk_write(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf)
{
    struct block_device *bdev = (struct block_device *)inode->private;
    if (!bdev) return -1;

    uint64_t sector = offset / bdev->sector_size;
    uint64_t byte_off = offset % bdev->sector_size;
    uint64_t to_write = size;
    uint8_t tmp[512];
    uint64_t done = 0;

    if (sector >= bdev->sector_count) return -1;
    if (to_write > bdev->sector_count * bdev->sector_size - offset)
        to_write = bdev->sector_count * bdev->sector_size - offset;

    if (byte_off)
    {
        if (block_read(bdev, sector, 1, tmp) != 0) return -1;
        uint64_t chunk = bdev->sector_size - byte_off;
        if (chunk > to_write) chunk = to_write;
        for (uint64_t i = 0; i < chunk; i++)
            tmp[byte_off + i] = ((const uint8_t *)buf)[done++];
        if (block_write(bdev, sector, 1, tmp) != 0) return (int)done;
        sector++;
        to_write -= chunk;
    }

    while (to_write >= bdev->sector_size)
    {
        uint8_t cnt = 1;
        if (block_write(bdev, sector, cnt, &((const uint8_t *)buf)[done]) != 0) return (int)done;
        done += bdev->sector_size;
        sector++;
        to_write -= bdev->sector_size;
    }

    if (to_write)
    {
        if (block_read(bdev, sector, 1, tmp) != 0) return (int)done;
        for (uint64_t i = 0; i < to_write; i++)
            tmp[i] = ((const uint8_t *)buf)[done++];
        if (block_write(bdev, sector, 1, tmp) != 0) return (int)done;
    }

    return (int)done;
}

static struct vfs_sb_ops devfs_sb_ops = {
    .alloc_inode   = devfs_alloc_inode,
    .destroy_inode = devfs_destroy_inode,
    .get_root      = NULL,
};

int vfs_mount_devfs(const char *path)
{
    (void)path;

    struct devfs_data *d = &devfs_data;
    d->entry_count = 0;
    d->next_ino = 1;

    struct devfs_entry *dot = &d->entries[d->entry_count++];
    int n;
    for (n = 0; "."[n]; n++) dot->name[n] = "."[n];
    dot->name[n] = '\0';
    dot->ino   = 0;
    dot->type  = VFS_DIR;
    dot->size  = 0;
    dot->private = NULL;

    int bcount = block_count();
    for (int i = 0; i < bcount && d->entry_count < DEVFS_MAX_ENTRIES; i++)
    {
        struct block_device *bdev = block_get(i);
        if (!bdev) continue;

        struct devfs_entry *e = &d->entries[d->entry_count++];
        int k;
        for (k = 0; bdev->name[k] && k < VFS_NAME_MAX - 1; k++)
            e->name[k] = bdev->name[k];
        e->name[k] = '\0';
        e->ino   = d->next_ino++;
        e->type  = VFS_BLKDEV;
        e->size  = bdev->sector_count * bdev->sector_size;
        e->private = bdev;
    }

    devfs_root_inode.ino      = 0;
    devfs_root_inode.type     = VFS_DIR;
    devfs_root_inode.size     = 0;
    devfs_root_inode.sb       = &devfs_sb;
    devfs_root_inode.ops      = &devfs_dir_ops;
    devfs_root_inode.private  = d;
    devfs_root_inode.refcount = 1;

    devfs_sb.mounted  = 1;
    devfs_sb.sb_ops   = &devfs_sb_ops;
    devfs_sb.root     = &devfs_root_inode;
    devfs_sb.private  = d;

    return vfs_mount("/", &devfs_sb);
}
