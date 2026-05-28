#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#define SYS_read    0
#define SYS_write   1
#define SYS_open    2
#define SYS_close   3
#define SYS_readdir 5
#define SYS_stat    6
#define SYS_clear   7
#define SYS_reboot  8
#define SYS_brk     45
#define SYS_spawn   59
#define SYS_exit    60
#define SYS_waitpid 61

struct vfs_dentry;

static inline long syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}

static inline int spawn(const char *path, int argc, char **argv)
{
    return (int)syscall(SYS_spawn, (long)path, (long)argc, (long)argv);
}

static inline int waitpid(int pid)
{
    int code;
    while ((code = (int)syscall(SYS_waitpid, (long)pid, 0, 0)) == -2)
        ;
    return code;
}

static inline void *sbrk(long increment)
{
    long old = syscall(SYS_brk, 0, 0, 0);
    if (increment == 0)
        return (void *)old;
    long new = syscall(SYS_brk, old + increment, 0, 0);
    if (new == -1)
        return (void *)-1;
    return (void *)old;
}

static inline int open(const char *path, int flags)
{
    return (int)syscall(SYS_open, (long)path, (long)flags, 0);
}

static inline int close(int fd)
{
    return (int)syscall(SYS_close, (long)fd, 0, 0);
}

static inline int readdir(const char *path, unsigned int index, struct vfs_dentry *entry)
{
    return (int)syscall(SYS_readdir, (long)path, (long)index, (long)entry);
}

static inline int stat(const char *path, struct vfs_dentry *entry)
{
    return (int)syscall(SYS_stat, (long)path, (long)entry, 0);
}

static inline void clear(void)
{
    syscall(SYS_clear, 0, 0, 0);
}

static inline void reboot(void)
{
    syscall(SYS_reboot, 0, 0, 0);
}

#endif