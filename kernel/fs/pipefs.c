#include "vfs.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../drivers/serial/serial.h"
#include "../include/spinlock.h"
#include "../proc/task.h"
#include "pipefs.h"

static int pipe_read(struct vfs_inode *inode, uint64_t offset, uint64_t count, void *buf)
{
    (void)offset;
    struct pipe_inode *pipe = (struct pipe_inode *)inode->private;
    uint64_t total = 0;

    while (total < count)
    {
        if (pipe->read_pos != pipe->write_pos)
        {
            unsigned char c = pipe->buf[pipe->read_pos % PIPE_BUF_SIZE];
            pipe->read_pos++;
            ((unsigned char *)buf)[total++] = c;
            continue;
        }

        if (pipe->writers == 0 && total > 0)
            return total;

        if (pipe->writers == 0)
            return 0;

        __asm__ volatile("sti; hlt; cli");
    }

    return total;
}

static int pipe_write(struct vfs_inode *inode, uint64_t offset, uint64_t count, const void *buf)
{
    (void)offset;
    struct pipe_inode *pipe = (struct pipe_inode *)inode->private;
    uint64_t total = 0;

    while (total < count)
    {
        int next_pos = pipe->write_pos % PIPE_BUF_SIZE;
        int used = pipe->write_pos - pipe->read_pos;

        if (used < PIPE_BUF_SIZE)
        {
            pipe->buf[next_pos] = ((const unsigned char *)buf)[total++];
            pipe->write_pos++;
            continue;
        }

        if (pipe->readers == 0)
            return -1;

        __asm__ volatile("sti; hlt; cli");
    }

    return total;
}

static int pipe_truncate(struct vfs_inode *inode, uint64_t size)
{
    (void)inode;
    (void)size;
    return 0;
}

static struct vfs_inode_ops pipe_inode_ops = {
    .read     = pipe_read,
    .write    = pipe_write,
    .truncate = pipe_truncate,
};

int vfs_pipe_create(int *fd0, int *fd1)
{
    struct pipe_inode *pipe = (struct pipe_inode *)pmm_alloc();
    if (!pipe) return -1;

    for (int i = 0; i < PIPE_BUF_SIZE; i++)
        pipe->buf[i] = 0;
    pipe->read_pos  = 0;
    pipe->write_pos = 0;
    pipe->readers   = 1;
    pipe->writers   = 1;
    pipe->refcount  = 2;

    struct vfs_inode *inode = vfs_alloc_inode();
    if (!inode)
    {
        pmm_free(pipe);
        return -1;
    }

    inode->type     = VFS_FILE;
    inode->size     = PIPE_BUF_SIZE;
    inode->ops      = &pipe_inode_ops;
    inode->private  = pipe;
    inode->refcount = 2;

    struct vfs_file *f0 = vfs_alloc_file();
    struct vfs_file *f1 = vfs_alloc_file();
    if (!f0 || !f1)
    {
        inode->refcount = 0;
        if (f0) vfs_free_file(f0);
        if (f1) vfs_free_file(f1);
        pmm_free(pipe);
        return -1;
    }

    f0->inode  = inode;
    f0->offset = 0;
    f0->flags  = VFS_O_READ;

    f1->inode  = inode;
    f1->offset = 0;
    f1->flags  = VFS_O_WRITE;

    int f0_found = -1, f1_found = -1;

    extern struct task *current_task;
    for (int i = 0; i < MAX_FDS; i++)
    {
        if (!current_task->fds[i])
        {
            if (f0_found < 0)
            {
                f0_found = i;
                current_task->fds[i] = f0;
            }
            else if (f1_found < 0)
            {
                f1_found = i;
                current_task->fds[i] = f1;
                break;
            }
        }
    }

    if (f1_found < 0)
    {
        vfs_close(f0);
        vfs_close(f1);
        return -1;
    }

    *fd0 = f0_found;
    *fd1 = f1_found;
    return 0;
}
