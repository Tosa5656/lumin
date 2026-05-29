#include "vfs.h"
#include "../mm/kmalloc.h"
#include "../drivers/serial/serial.h"
#include "kprintf.h"
#include <stddef.h>

#define TMPFS_MAX_ENTRIES 64
#define TMPFS_MAX_FILE_SIZE 65536

struct tmpfs_child {
    char    name[VFS_NAME_MAX];
    struct vfs_inode *inode;
};

struct tmpfs_dir {
    struct tmpfs_child children[TMPFS_MAX_ENTRIES];
    int child_count;
};

struct tmpfs_file {
    uint8_t *data;
    uint64_t size;
    uint64_t capacity;
};

static struct vfs_superblock tmpfs_sb;
static struct vfs_inode tmpfs_root_inode;

static struct tmpfs_dir *tmpfs_alloc_dir(void)
{
    struct tmpfs_dir *d = (struct tmpfs_dir *)kmalloc(sizeof(struct tmpfs_dir));
    if (!d) return NULL;
    d->child_count = 0;
    return d;
}

static struct tmpfs_file *tmpfs_alloc_file(void)
{
    struct tmpfs_file *f = (struct tmpfs_file *)kmalloc(sizeof(struct tmpfs_file));
    if (!f) return NULL;
    f->data = NULL;
    f->size = 0;
    f->capacity = 0;
    return f;
}

static int tmpfs_free_file(struct tmpfs_file *f)
{
    if (!f) return -1;
    if (f->data) kfree(f->data);
    kfree(f);
    return 0;
}

static int tmpfs_read(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    if (!inode || inode->type != VFS_FILE) return -1;
    struct tmpfs_file *f = (struct tmpfs_file *)inode->private;
    if (!f || !f->data) return -1;

    if (offset >= f->size) return 0;
    uint64_t avail = f->size - offset;
    if (avail > size) avail = size;
    for (uint64_t i = 0; i < avail; i++)
        ((uint8_t *)buf)[i] = f->data[offset + i];
    return (int)avail;
}

static int tmpfs_write(struct vfs_inode *inode, uint64_t offset, uint64_t size, const void *buf)
{
    if (!inode || inode->type != VFS_FILE) return -1;
    struct tmpfs_file *f = (struct tmpfs_file *)inode->private;
    if (!f) return -1;

    uint64_t needed = offset + size;
    if (needed > f->capacity)
    {
        uint64_t new_cap = f->capacity ? f->capacity : 64;
        while (new_cap < needed && new_cap < TMPFS_MAX_FILE_SIZE)
            new_cap *= 2;
        if (new_cap > TMPFS_MAX_FILE_SIZE) new_cap = TMPFS_MAX_FILE_SIZE;
        if (new_cap < needed) return -1;

        uint8_t *new_data = (uint8_t *)kmalloc(new_cap);
        if (!new_data) return -1;

        if (f->data)
        {
            for (uint64_t i = 0; i < f->size; i++)
                new_data[i] = f->data[i];
            kfree(f->data);
        }
        f->data = new_data;
        f->capacity = new_cap;
    }

    for (uint64_t i = 0; i < size; i++)
        f->data[offset + i] = ((const uint8_t *)buf)[i];

    if (needed > f->size)
        f->size = needed;
    if (offset + size > inode->size)
        inode->size = offset + size;
    return (int)size;
}

static int tmpfs_truncate(struct vfs_inode *inode, uint64_t size)
{
    if (!inode || inode->type != VFS_FILE) return -1;
    struct tmpfs_file *f = (struct tmpfs_file *)inode->private;
    if (!f) return -1;

    if (size > f->capacity)
    {
        uint64_t new_cap = f->capacity ? f->capacity : 64;
        while (new_cap < size && new_cap < TMPFS_MAX_FILE_SIZE)
            new_cap *= 2;
        if (new_cap > TMPFS_MAX_FILE_SIZE) new_cap = TMPFS_MAX_FILE_SIZE;
        if (new_cap < size) return -1;

        uint8_t *new_data = (uint8_t *)kmalloc(new_cap);
        if (!new_data) return -1;

        if (f->data)
        {
            uint64_t copy = f->size < size ? f->size : size;
            for (uint64_t i = 0; i < copy; i++)
                new_data[i] = f->data[i];
            kfree(f->data);
        }
        f->data = new_data;
        f->capacity = new_cap;
    }

    f->size = size;
    inode->size = size;
    return 0;
}

static int tmpfs_readdir(struct vfs_inode *dir, uint32_t index, struct vfs_dentry *entry)
{
    if (!dir || dir->type != VFS_DIR) return -1;
    struct tmpfs_dir *d = (struct tmpfs_dir *)dir->private;
    if (!d || index >= (uint32_t)d->child_count)
        return -1;

    int k;
    for (k = 0; d->children[index].name[k] && k < VFS_NAME_MAX - 1; k++)
        entry->name[k] = d->children[index].name[k];
    entry->name[k] = '\0';

    struct vfs_inode *child = d->children[index].inode;
    entry->ino  = child ? child->ino : 0;
    entry->type = child ? child->type : VFS_FILE;
    entry->size = child ? child->size : 0;
    return 0;
}

static int tmpfs_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **child)
{
    if (!dir || !name || !child || dir->type != VFS_DIR)
        return -1;

    struct tmpfs_dir *d = (struct tmpfs_dir *)dir->private;
    if (!d) return -1;

    for (int i = 0; i < d->child_count; i++)
    {
        int match = 1;
        const char *a = d->children[i].name;
        const char *b = name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a != *b) match = 0;

        if (match)
        {
            *child = d->children[i].inode;
            if (*child) (*child)->refcount++;
            return 0;
        }
    }
    return -1;
}

static int tmpfs_create(struct vfs_inode *dir, const char *name, struct vfs_inode **child)
{
    if (!dir || !name || dir->type != VFS_DIR)
        return -1;

    struct tmpfs_dir *d = (struct tmpfs_dir *)dir->private;
    if (!d || d->child_count >= TMPFS_MAX_ENTRIES)
        return -1;

    struct vfs_inode *inode = (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
    if (!inode) return -1;

    struct tmpfs_file *f = tmpfs_alloc_file();
    if (!f)
    {
        kfree(inode);
        return -1;
    }

    inode->ino      = (uint64_t)(uintptr_t)inode;
    inode->type     = VFS_FILE;
    inode->size     = 0;
    inode->sb       = dir->sb;
    inode->private  = f;
    inode->refcount = 1;

    struct tmpfs_child *c = &d->children[d->child_count++];
    int k;
    for (k = 0; name[k] && k < VFS_NAME_MAX - 1; k++)
        c->name[k] = name[k];
    c->name[k] = '\0';
    c->inode = inode;

    if (child) *child = inode;
    return 0;
}

static int tmpfs_unlink(struct vfs_inode *dir, const char *name)
{
    if (!dir || !name || dir->type != VFS_DIR)
        return -1;

    struct tmpfs_dir *d = (struct tmpfs_dir *)dir->private;
    if (!d) return -1;

    for (int i = 0; i < d->child_count; i++)
    {
        int match = 1;
        const char *a = d->children[i].name;
        const char *b = name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a != *b) match = 0;

        if (match)
        {
            struct vfs_inode *inode = d->children[i].inode;
            if (inode && inode->private)
            {
                if (inode->type == VFS_FILE)
                    tmpfs_free_file((struct tmpfs_file *)inode->private);
            }
            if (inode) kfree(inode);

            for (int j = i; j < d->child_count - 1; j++)
                d->children[j] = d->children[j + 1];
            d->child_count--;
            return 0;
        }
    }
    return -1;
}

static int tmpfs_mkdir(struct vfs_inode *dir, const char *name)
{
    if (!dir || !name || dir->type != VFS_DIR)
        return -1;

    struct tmpfs_dir *d = (struct tmpfs_dir *)dir->private;
    if (!d || d->child_count >= TMPFS_MAX_ENTRIES)
        return -1;

    struct tmpfs_dir *child_dir = tmpfs_alloc_dir();
    if (!child_dir) return -1;

    struct vfs_inode *inode = (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
    if (!inode)
    {
        kfree(child_dir);
        return -1;
    }

    inode->ino      = (uint64_t)(uintptr_t)inode;
    inode->type     = VFS_DIR;
    inode->size     = 0;
    inode->sb       = dir->sb;
    inode->private  = child_dir;
    inode->refcount = 1;

    struct tmpfs_child *c = &d->children[d->child_count++];
    int k;
    for (k = 0; name[k] && k < VFS_NAME_MAX - 1; k++)
        c->name[k] = name[k];
    c->name[k] = '\0';
    c->inode = inode;

    return 0;
}

static struct vfs_inode_ops tmpfs_file_ops = {
    .read     = tmpfs_read,
    .write    = tmpfs_write,
    .truncate = tmpfs_truncate,
};

static struct vfs_inode_ops tmpfs_dir_ops = {
    .readdir  = tmpfs_readdir,
    .lookup   = tmpfs_lookup,
    .create   = tmpfs_create,
    .unlink   = tmpfs_unlink,
    .mkdir    = tmpfs_mkdir,
};

static void tmpfs_destroy_inode(struct vfs_inode *inode)
{
    if (!inode || inode == &tmpfs_root_inode) return;
    if (inode->private)
    {
        if (inode->type == VFS_FILE)
            tmpfs_free_file((struct tmpfs_file *)inode->private);
        else if (inode->type == VFS_DIR)
            kfree(inode->private);
    }
    kfree(inode);
}

static struct vfs_inode *tmpfs_alloc_inode(struct vfs_superblock *sb)
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

static struct vfs_sb_ops tmpfs_sb_ops = {
    .alloc_inode   = tmpfs_alloc_inode,
    .destroy_inode = tmpfs_destroy_inode,
    .get_root      = NULL,
};

int vfs_mount_tmpfs(const char *path)
{
    if (!path) return -1;

    struct tmpfs_dir *root_dir = tmpfs_alloc_dir();
    if (!root_dir) return -1;

    tmpfs_root_inode.ino      = 1;
    tmpfs_root_inode.type     = VFS_DIR;
    tmpfs_root_inode.size     = 0;
    tmpfs_root_inode.sb       = &tmpfs_sb;
    tmpfs_root_inode.ops      = &tmpfs_dir_ops;
    tmpfs_root_inode.private  = root_dir;
    tmpfs_root_inode.refcount = 1;

    tmpfs_sb.mounted = 1;
    tmpfs_sb.sb_ops  = &tmpfs_sb_ops;
    tmpfs_sb.root    = &tmpfs_root_inode;
    tmpfs_sb.private = root_dir;

    serial_printf("tmpfs: mounted at '%s'\n", path);
    return vfs_mount(path, &tmpfs_sb);
}