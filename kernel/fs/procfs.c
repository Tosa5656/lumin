#include "vfs.h"
#include "../mm/kmalloc.h"
#include "../mm/pmm.h"
#include "../proc/task.h"
#include "../drivers/serial/serial.h"
#include "../drivers/timer/lapic.h"
#include "../drivers/timer/pit.h"
#include "kprintf.h"
#include <stddef.h>

#define PROCFS_MAX_ENTRIES 32

struct procfs_entry {
    char    name[VFS_NAME_MAX];
    uint64_t ino;
    enum vfs_inode_type type;
    int     (*read)(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf);
};

struct procfs_data {
    struct procfs_entry entries[PROCFS_MAX_ENTRIES];
    int entry_count;
    int next_ino;
};

static struct procfs_data procfs_data;
static struct vfs_inode procfs_root_inode;
static struct vfs_superblock procfs_sb;

static int procfs_read_stat(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    (void)inode;
    char tmp[256];
    uint64_t ticks = lapic_get_ticks();
    uint64_t uptime_sec = ticks / 1000;
    int n = 0;
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "cpu  0 0 0 0 0 0 0 0 0 0\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "intr 0 0 0 0 0 0 0 0 0 0\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "ctxt 0\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "btime %llu\n", (unsigned long long)uptime_sec);
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "processes %d\n", 1);
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "procs_running 1\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n, "procs_blocked 0\n");

    if (offset >= (uint64_t)n) return 0;
    uint64_t avail = (uint64_t)n - offset;
    if (avail > size) avail = size;
    for (uint64_t i = 0; i < avail; i++)
        ((uint8_t *)buf)[i] = tmp[offset + i];
    return (int)avail;
}

static int procfs_read_uptime(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    (void)inode;
    uint64_t ticks = lapic_get_ticks();
    uint64_t uptime_sec = ticks / 1000;
    uint64_t uptime_us = (ticks % 1000) * 1000UL;

    char tmp[64];
    int n = ksnprintf(tmp, sizeof(tmp), "%llu.%06llu 0.00\n",
                      (unsigned long long)uptime_sec,
                      (unsigned long long)uptime_us);

    if (offset >= (uint64_t)n) return 0;
    uint64_t avail = (uint64_t)n - offset;
    if (avail > size) avail = size;
    for (uint64_t i = 0; i < avail; i++)
        ((uint8_t *)buf)[i] = tmp[offset + i];
    return (int)avail;
}

static int procfs_read_maps(struct vfs_inode *inode, uint64_t offset, uint64_t size, void *buf)
{
    (void)inode;
    char tmp[512];
    int n = 0;

    n += ksnprintf(tmp + n, sizeof(tmp) - n,
                   "ffff800000000000-ffff800000200000 r-xp 00000000 00:00 0  /kernel\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n,
                   "ffff800000200000-ffff800000300000 rw-p 00200000 00:00 0  /kernel\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n,
                   "ffff800000300000-ffff800000400000 rw-p 00000000 00:00 0  [heap]\n");
    n += ksnprintf(tmp + n, sizeof(tmp) - n,
                   "ffff800000800000-ffff800000900000 rw-p 00000000 00:00 0  [stack]\n");

    if (offset >= (uint64_t)n) return 0;
    uint64_t avail = (uint64_t)n - offset;
    if (avail > size) avail = size;
    for (uint64_t i = 0; i < avail; i++)
        ((uint8_t *)buf)[i] = tmp[offset + i];
    return (int)avail;
}

static int procfs_dir_readdir(struct vfs_inode *dir, uint32_t index, struct vfs_dentry *entry)
{
    struct procfs_data *d = (struct procfs_data *)dir->private;
    if (!d || index >= (uint32_t)d->entry_count)
        return -1;

    int k;
    for (k = 0; d->entries[index].name[k] && k < VFS_NAME_MAX - 1; k++)
        entry->name[k] = d->entries[index].name[k];
    entry->name[k] = '\0';
    entry->ino  = d->entries[index].ino;
    entry->type = d->entries[index].type;
    entry->size = 4096;
    return 0;
}

static int procfs_dir_lookup(struct vfs_inode *dir, const char *name, struct vfs_inode **child)
{
    struct procfs_data *d = (struct procfs_data *)dir->private;
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
            struct vfs_inode *inode = (struct vfs_inode *)kmalloc(sizeof(struct vfs_inode));
            if (!inode) return -1;

            inode->ino      = d->entries[i].ino;
            inode->type     = d->entries[i].type;
            inode->size     = 4096;
            inode->sb       = dir->sb;
            inode->private  = d;
            inode->refcount = 1;

            if (inode->type == VFS_DIR)
            {
                static struct vfs_inode_ops dir_ops;
                dir_ops.readdir = procfs_dir_readdir;
                dir_ops.lookup  = procfs_dir_lookup;
                dir_ops.read    = NULL;
                dir_ops.write   = NULL;
                inode->ops = &dir_ops;
            }
            else
            {
                static struct vfs_inode_ops file_ops;
                file_ops.read    = d->entries[i].read;
                file_ops.write   = NULL;
                file_ops.readdir = NULL;
                file_ops.lookup  = NULL;
                inode->ops = &file_ops;
            }

            *child = inode;
            return 0;
        }
    }
    return -1;
}

static struct vfs_inode_ops procfs_dir_ops;

static void procfs_destroy_inode(struct vfs_inode *inode)
{
    if (!inode || inode == &procfs_root_inode) return;
    kfree(inode);
}

static struct vfs_inode *procfs_alloc_inode(struct vfs_superblock *sb)
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

static struct vfs_sb_ops procfs_sb_ops = {
    .alloc_inode   = procfs_alloc_inode,
    .destroy_inode = procfs_destroy_inode,
    .get_root      = NULL,
};

int vfs_mount_procfs(const char *path)
{
    if (!path) return -1;

    struct procfs_data *d = &procfs_data;
    d->entry_count = 0;
    d->next_ino = 1;

    {
        struct procfs_entry *e = &d->entries[d->entry_count++];
        e->name[0] = '.'; e->name[1] = '\0';
        e->ino  = 0;
        e->type = VFS_DIR;
        e->read = NULL;
    }
    {
        struct procfs_entry *e = &d->entries[d->entry_count++];
        e->name[0] = '.'; e->name[1] = '.'; e->name[2] = '\0';
        e->ino  = 0;
        e->type = VFS_DIR;
        e->read = NULL;
    }
    {
        struct procfs_entry *e = &d->entries[d->entry_count++];
        e->name[0] = 's'; e->name[1] = 't'; e->name[2] = 'a'; e->name[3] = 't'; e->name[4] = '\0';
        e->ino  = d->next_ino++;
        e->type = VFS_FILE;
        e->read = procfs_read_stat;
    }
    {
        struct procfs_entry *e = &d->entries[d->entry_count++];
        e->name[0] = 'u'; e->name[1] = 'p'; e->name[2] = 't'; e->name[3] = 'i';
        e->name[4] = 'm'; e->name[5] = 'e'; e->name[6] = '\0';
        e->ino  = d->next_ino++;
        e->type = VFS_FILE;
        e->read = procfs_read_uptime;
    }
    {
        struct procfs_entry *e = &d->entries[d->entry_count++];
        e->name[0] = 's'; e->name[1] = 'e'; e->name[2] = 'l'; e->name[3] = 'f'; e->name[4] = '\0';
        e->ino  = d->next_ino++;
        e->type = VFS_DIR;
        e->read = NULL;
    }

    int self_idx = d->entry_count - 1;
    {
        struct procfs_entry *e = &d->entries[d->entry_count++];
        e->name[0] = 'm'; e->name[1] = 'a'; e->name[2] = 'p'; e->name[3] = 's'; e->name[4] = '\0';
        e->ino  = d->next_ino++;
        e->type = VFS_FILE;
        e->read = procfs_read_maps;
    }

    procfs_dir_ops.readdir = procfs_dir_readdir;
    procfs_dir_ops.lookup  = procfs_dir_lookup;
    procfs_dir_ops.read    = NULL;
    procfs_dir_ops.write   = NULL;
    procfs_dir_ops.truncate = NULL;
    procfs_dir_ops.create  = NULL;
    procfs_dir_ops.unlink  = NULL;
    procfs_dir_ops.mkdir   = NULL;

    procfs_root_inode.ino      = 0;
    procfs_root_inode.type     = VFS_DIR;
    procfs_root_inode.size     = 0;
    procfs_root_inode.sb       = &procfs_sb;
    procfs_root_inode.ops      = &procfs_dir_ops;
    procfs_root_inode.private  = d;
    procfs_root_inode.refcount = 1;

    procfs_sb.mounted = 1;
    procfs_sb.sb_ops  = &procfs_sb_ops;
    procfs_sb.root    = &procfs_root_inode;
    procfs_sb.private = d;

    serial_write("procfs: mounted\n");
    return vfs_mount(path, &procfs_sb);
}
