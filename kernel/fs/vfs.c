#include "vfs.h"
#include "drivers/serial/serial.h"
#include "block/block.h"
#include "mm/kmalloc.h"
#include <stddef.h>

void *memcpy(void *dst, const void *src, unsigned long n)
{
    for (unsigned long i = 0; i < n; i++)
        ((char *)dst)[i] = ((const char *)src)[i];
    return dst;
}

static struct vfs_file file_table[VFS_FILE_MAX];
static int file_count;

struct vfs_mount_entry {
    char path[VFS_PATH_MAX];
    struct vfs_superblock sb;
};
static struct vfs_mount_entry mounts[VFS_MOUNT_MAX];
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

static int path_match(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static int resolve_full(const char *path, struct vfs_inode **out_inode)
{
    if (!path || *path == '\0' || !out_inode)
        return -1;
    if (mount_count == 0)
        return -1;

    struct vfs_inode *cur = mounts[0].sb.root;
    if (!cur) return -1;
    vfs_inode_ref(cur);

    char buf[VFS_PATH_MAX];
    char fullpath[VFS_PATH_MAX];
    int fp_len = 0;

    int i = 0;

    if (path[0] != '/')
    {
        fullpath[fp_len++] = '/';
    }

    while (path[i] != '\0')
    {
        while (path[i] == '/')
        {
            if (fp_len < VFS_PATH_MAX - 1)
                fullpath[fp_len++] = path[i];
            i++;
        }
        if (path[i] == '\0') break;

        int j = 0;
        while (path[i] != '/' && path[i] != '\0' && j < VFS_NAME_MAX - 1)
            buf[j++] = path[i++];
        buf[j] = '\0';

        for (int k = 0; k < j && fp_len < VFS_PATH_MAX - 1; k++)
            fullpath[fp_len++] = buf[k];
        fullpath[fp_len] = '\0';

        struct vfs_inode *mnt_root = NULL;
        for (int mi = 1; mi < mount_count; mi++)
        {
            if (path_match(mounts[mi].path, fullpath))
            {
                mnt_root = mounts[mi].sb.root;
                break;
            }
        }

        if (mnt_root)
        {
            vfs_inode_unref(cur);
            cur = mnt_root;
            vfs_inode_ref(cur);
        }
        else
        {
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
    }

    fullpath[fp_len] = '\0';

    for (int mi = mount_count - 1; mi >= 0; mi--)
    {
        if (path_match(mounts[mi].path, fullpath))
        {
            if (mounts[mi].sb.root && mounts[mi].sb.root != cur)
            {
                vfs_inode_unref(cur);
                cur = mounts[mi].sb.root;
                vfs_inode_ref(cur);
            }
            break;
        }
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
    mounts[0].path[0] = '/';
    mounts[0].path[1] = '\0';
    serial_write("vfs: initialized\n");
    return 0;
}

int vfs_mount(const char *path, struct vfs_superblock *sb)
{
    if (!path || !sb || mount_count >= VFS_MOUNT_MAX)
        return -1;
    if (!sb->root || !sb->sb_ops)
        return -1;

    int n;
    for (n = 0; path[n] && n < VFS_PATH_MAX - 1; n++)
        mounts[mount_count].path[n] = path[n];
    mounts[mount_count].path[n] = '\0';

    sb->mounted = 1;
    mounts[mount_count].sb = *sb;
    mount_count++;
    serial_printf("vfs: mounted '%s'\n", path);
    return 0;
}

struct vfs_file *vfs_open(const char *path, int flags)
{
    if (!path)
        return NULL;

    struct vfs_inode *inode = NULL;
    if (resolve_full(path, &inode) != 0)
    {
        if ((flags & VFS_O_CREAT) && vfs_create(path) == 0)
        {
            if (resolve_full(path, &inode) != 0)
                return NULL;
        }
        else
        {
            return NULL;
        }
    }

    int idx = -1;
    for (int i = 0; i < VFS_FILE_MAX; i++)
    {
        if (file_table[i].inode == NULL)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
    {
        vfs_inode_unref(inode);
        return NULL;
    }

    if (idx >= file_count)
        file_count = idx + 1;

    struct vfs_file *f = &file_table[idx];
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

struct vfs_file *vfs_dup(struct vfs_file *file)
{
    if (!file || !file->inode)
        return NULL;

    int idx = -1;
    for (int i = 0; i < VFS_FILE_MAX; i++)
    {
        if (file_table[i].inode == NULL)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return NULL;

    if (idx >= file_count)
        file_count = idx + 1;

    struct vfs_file *f = &file_table[idx];
    vfs_inode_ref(file->inode);
    f->inode  = file->inode;
    f->offset = file->offset;
    f->flags  = file->flags;
    return f;
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

    uint32_t fs_count = 0;
    {
        struct vfs_dentry tmp;
        while (inode->ops->readdir(inode, fs_count, &tmp) == 0)
            fs_count++;
    }

    if (index < fs_count)
    {
        int r = inode->ops->readdir(inode, index, entry);
        vfs_inode_unref(inode);
        return r;
    }

    vfs_inode_unref(inode);

    int path_len = 0;
    while (path[path_len]) path_len++;

    uint32_t mnt_idx = 0;
    for (int mi = 1; mi < mount_count; mi++)
    {
        const char *mp = mounts[mi].path;
        int j;
        for (j = 0; j < path_len; j++)
            if (mp[j] != path[j]) break;
        if (j != path_len) continue;

        const char *child = mp + j;
        while (*child == '/') child++;
        if (*child == '\0') continue;
        int is_direct = 1;
        for (int k = 0; child[k]; k++)
            if (child[k] == '/') { is_direct = 0; break; }
        if (!is_direct) continue;

        if (mnt_idx == index - fs_count)
        {
            int n;
            for (n = 0; child[n] && n < VFS_NAME_MAX - 1; n++)
                entry->name[n] = child[n];
            entry->name[n] = '\0';
            entry->ino  = 0;
            entry->type = VFS_DIR;
            entry->size = 0;
            return 0;
        }
        mnt_idx++;
    }

    return -1;
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

static int resolve_parent(const char *path, struct vfs_inode **out_parent, char *child_name)
{
    if (!path || *path != '/' || !out_parent || !child_name)
        return -1;

    char buf[VFS_PATH_MAX];
    int i = 0;
    while (path[i] != '\0') { buf[i] = path[i]; i++; }
    buf[i] = '\0';

    char *slash = buf + i - 1;
    while (slash > buf && *slash == '/') slash--;
    while (slash > buf && *slash != '/') slash--;

    char parent_path[VFS_PATH_MAX];
    int pn = 0;
    for (int j = 0; j < slash - buf + 1; j++)
        parent_path[pn++] = buf[j];
    if (pn == 0) { parent_path[pn++] = '/'; }
    parent_path[pn] = '\0';

    if (resolve_full(parent_path, out_parent) != 0)
        return -1;

    int cn = 0;
    const char *cp = (slash[0] == '/') ? slash + 1 : slash;
    while (*cp && *cp == '/') cp++;
    while (*cp && *cp != '/' && cn < VFS_NAME_MAX - 1)
        child_name[cn++] = *cp++;
    child_name[cn] = '\0';

    return 0;
}

int vfs_truncate(const char *path, uint64_t size)
{
    if (!path) return -1;
    struct vfs_inode *inode = NULL;
    if (resolve_full(path, &inode) != 0) return -1;
    if (!inode->ops || !inode->ops->truncate)
    {
        vfs_inode_unref(inode);
        return -1;
    }
    int r = inode->ops->truncate(inode, size);
    vfs_inode_unref(inode);
    return r;
}

int vfs_create(const char *path)
{
    if (!path) return -1;
    struct vfs_inode *parent = NULL;
    char child_name[VFS_NAME_MAX];
    if (resolve_parent(path, &parent, child_name) != 0)
        return -1;
    if (parent->type != VFS_DIR || !parent->ops || !parent->ops->create)
    {
        vfs_inode_unref(parent);
        return -1;
    }
    int r = parent->ops->create(parent, child_name, NULL);
    vfs_inode_unref(parent);
    return r;
}

int vfs_unlink(const char *path)
{
    if (!path) return -1;
    struct vfs_inode *parent = NULL;
    char child_name[VFS_NAME_MAX];
    if (resolve_parent(path, &parent, child_name) != 0)
        return -1;
    if (parent->type != VFS_DIR || !parent->ops || !parent->ops->unlink)
    {
        vfs_inode_unref(parent);
        return -1;
    }
    int r = parent->ops->unlink(parent, child_name);
    vfs_inode_unref(parent);
    return r;
}

int vfs_mkdir(const char *path)
{
    if (!path) return -1;
    struct vfs_inode *parent = NULL;
    char child_name[VFS_NAME_MAX];
    if (resolve_parent(path, &parent, child_name) != 0)
        return -1;
    if (parent->type != VFS_DIR || !parent->ops || !parent->ops->mkdir)
    {
        vfs_inode_unref(parent);
        return -1;
    }
    int r = parent->ops->mkdir(parent, child_name);
    vfs_inode_unref(parent);
    return r;
}


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

static struct devfs_data devfs_root_data;
static struct devfs_data devfs_dev_data;
static struct vfs_inode  devfs_root_inode;
static struct vfs_inode  devfs_dev_inode;
static struct vfs_superblock devfs_sb;
static struct vfs_superblock devfs_dev_sb;

static void devfs_destroy_inode(struct vfs_inode *inode)
{
    if (!inode || inode == &devfs_root_inode || inode == &devfs_dev_inode) return;
    kfree(inode);
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

static int devfs_chr_read(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf);
static int devfs_chr_write(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf);
static struct vfs_inode_ops devfs_chr_ops = {
    .read     = devfs_chr_read,
    .write    = devfs_chr_write,
};

static int devfs_chr_read(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    (void)offset;
    struct vfs_inode_ops *ops = (struct vfs_inode_ops *)inode->private;
    if (!ops || !ops->read) return -1;
    return ops->read(inode, 0, size, buf);
}

static int devfs_chr_write(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf)
{
    (void)offset;
    struct vfs_inode_ops *ops = (struct vfs_inode_ops *)inode->private;
    if (!ops || !ops->write) return -1;
    return ops->write(inode, 0, size, buf);
}

static struct vfs_inode *devfs_alloc_inode(struct vfs_superblock *sb)
{
    (void)sb;
    struct vfs_inode *in = (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
    if (in) {
        in->ino = 0;
        in->type = VFS_FILE;
        in->size = 0;
        in->sb = NULL;
        in->ops = NULL;
        in->private = NULL;
        in->refcount = 1;
    }
    return in;
}

struct vfs_inode *vfs_alloc_inode(void)
{
    return devfs_alloc_inode(NULL);
}

struct vfs_file *vfs_alloc_file(void)
{
    int idx = -1;
    for (int i = 0; i < VFS_FILE_MAX; i++)
    {
        if (file_table[i].inode == NULL)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0) return NULL;
    if (idx >= file_count) file_count = idx + 1;
    file_table[idx].inode = NULL;
    file_table[idx].offset = 0;
    file_table[idx].flags = 0;
    return &file_table[idx];
}

void vfs_free_file(struct vfs_file *f)
{
    if (!f) return;
    f->inode = NULL;
    f->offset = 0;
    f->flags = 0;
}

int devfs_register_chrdev(const char *name, struct vfs_inode_ops *ops, void *priv)
{
    (void)priv;
    struct devfs_data *dev = &devfs_dev_data;
    if (dev->entry_count >= DEVFS_MAX_ENTRIES)
        return -1;

    struct devfs_entry *e = &dev->entries[dev->entry_count++];
    int k;
    for (k = 0; name[k] && k < VFS_NAME_MAX - 1; k++)
        e->name[k] = name[k];
    e->name[k] = '\0';
    e->ino   = dev->next_ino++;
    e->type  = VFS_CHRDEV;
    e->size  = 0;
    e->private = ops;
    serial_printf("devfs: registered char device '/dev/%s'\n", name);
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
                inode->private = d->entries[i].private ? d->entries[i].private : d;
            }
            else if (inode->type == VFS_BLKDEV)
            {
                static struct vfs_inode_ops blk_ops = {
                    .read     = devfs_blk_read,
                    .write    = devfs_blk_write,
                    .truncate = NULL,
                    .create   = NULL,
                    .unlink   = NULL,
                    .mkdir    = NULL,
                };
                inode->ops = &blk_ops;
            }
            else if (inode->type == VFS_CHRDEV)
            {
                inode->ops = &devfs_chr_ops;
            }

            *child = inode;
            return 0;
        }
    }
    return -1;
}

static struct vfs_inode_ops devfs_dir_ops = {
    .readdir  = devfs_dir_readdir,
    .lookup   = devfs_dir_lookup,
    .create   = NULL,
    .unlink   = NULL,
    .mkdir    = NULL,
    .truncate = NULL,
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
    if (!path) return -1;

    struct devfs_data *root = &devfs_root_data;
    root->entry_count = 0;
    root->next_ino = 1;

    {
        struct devfs_entry *e = &root->entries[root->entry_count++];
        e->name[0] = '.';
        e->name[1] = '\0';
        e->ino   = 0;
        e->type  = VFS_DIR;
        e->size  = 0;
        e->private = root;
    }

    struct devfs_data *dev = &devfs_dev_data;
    dev->entry_count = 0;
    dev->next_ino = 1;

    {
        struct devfs_entry *e = &dev->entries[dev->entry_count++];
        e->name[0] = '.';
        e->name[1] = '\0';
        e->ino   = 0;
        e->type  = VFS_DIR;
        e->size  = 0;
        e->private = dev;
    }

    {
        struct devfs_entry *e = &dev->entries[dev->entry_count++];
        e->name[0] = '.';
        e->name[1] = '.';
        e->name[2] = '\0';
        e->ino   = 0;
        e->type  = VFS_DIR;
        e->size  = 0;
        e->private = root;
    }

    int bcount = block_count();
    for (int i = 0; i < bcount && dev->entry_count < DEVFS_MAX_ENTRIES; i++)
    {
        struct block_device *bdev = block_get(i);
        if (!bdev) continue;

        struct devfs_entry *e = &dev->entries[dev->entry_count++];
        int k;
        for (k = 0; bdev->name[k] && k < VFS_NAME_MAX - 1; k++)
            e->name[k] = bdev->name[k];
        e->name[k] = '\0';
        e->ino   = dev->next_ino++;
        e->type  = VFS_BLKDEV;
        e->size  = bdev->sector_count * bdev->sector_size;
        e->private = bdev;
    }

    if (path[0] == '/' && path[1] == '\0')
    {
        {
            struct devfs_entry *e = &root->entries[root->entry_count++];
            e->name[0] = 'd';
            e->name[1] = 'e';
            e->name[2] = 'v';
            e->name[3] = '\0';
            e->ino   = root->next_ino++;
            e->type  = VFS_DIR;
            e->size  = 0;
            e->private = dev;
        }

        devfs_root_inode.ino      = 0;
        devfs_root_inode.type     = VFS_DIR;
        devfs_root_inode.size     = 0;
        devfs_root_inode.sb       = &devfs_sb;
        devfs_root_inode.ops      = &devfs_dir_ops;
        devfs_root_inode.private  = root;
        devfs_root_inode.refcount = 1;

        devfs_sb.mounted  = 1;
        devfs_sb.sb_ops   = &devfs_sb_ops;
        devfs_sb.root     = &devfs_root_inode;
        devfs_sb.private  = root;

        return vfs_mount("/", &devfs_sb);
    }

    devfs_dev_inode.ino      = 0;
    devfs_dev_inode.type     = VFS_DIR;
    devfs_dev_inode.size     = 0;
    devfs_dev_inode.sb       = &devfs_dev_sb;
    devfs_dev_inode.ops      = &devfs_dir_ops;
    devfs_dev_inode.private  = dev;
    devfs_dev_inode.refcount = 1;

    devfs_dev_sb.mounted  = 1;
    devfs_dev_sb.sb_ops   = &devfs_sb_ops;
    devfs_dev_sb.root     = &devfs_dev_inode;
    devfs_dev_sb.private  = dev;

    return vfs_mount(path, &devfs_dev_sb);
}

int vfs_umount(const char *path)
{
    if (!path) return -1;
    for (int i = 1; i < mount_count; i++)
    {
        if (path_match(mounts[i].path, path))
        {
            for (int j = i; j < mount_count - 1; j++)
                __builtin_memcpy(&mounts[j], &mounts[j + 1], sizeof(struct vfs_mount_entry));
            mount_count--;
            serial_printf("vfs: unmounted '%s'\n", path);
            return 0;
        }
    }
    return -1;
}

struct block_device *vfs_get_block_device(const char *path)
{
    if (!path) return NULL;
    struct vfs_inode *inode = NULL;
    if (resolve_full(path, &inode) != 0)
        return NULL;
    if (inode->type != VFS_BLKDEV)
    {
        vfs_inode_unref(inode);
        return NULL;
    }
    struct block_device *bdev = (struct block_device *)inode->private;
    vfs_inode_unref(inode);
    return bdev;
}