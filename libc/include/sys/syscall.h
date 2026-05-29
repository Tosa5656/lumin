#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <errno.h>

#define SYS_read    0
#define SYS_write   1
#define SYS_open    2
#define SYS_close   3
#define SYS_pipe    4
#define SYS_readdir 5
#define SYS_stat    6
#define SYS_clear   7
#define SYS_reboot  8
#define SYS_dup     9
#define SYS_dup2    10
#define SYS_kill    11
#define SYS_sigaction 12
#define SYS_sigreturn 13
#define SYS_lseek   14
#define SYS_unlink  15
#define SYS_mkdir   16
#define SYS_rmdir   17
#define SYS_exec    18
#define SYS_nanosleep 35
#define SYS_yield   24
#define SYS_getpid  39
#define SYS_brk     45
#define SYS_fork    57
#define SYS_spawn   59
#define SYS_exit    60
#define SYS_waitpid 61
#define SYS_getcwd  79
#define SYS_chdir   80
#define SYS_shutdown 0x7F
#define SYS_clock_gettime 0x80

struct stat;
struct vfs_dentry;

static inline long __syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}

static inline long syscall(long n, long a1, long a2, long a3)
{
    return __syscall(n, a1, a2, a3);
}

static inline long __sysret(long ret)
{
    if (ret < 0) { errno = (int)-ret; return -1; }
    return ret;
}

static inline int spawn(const char *path, int argc, char **argv)
{
    return (int)__sysret(__syscall(SYS_spawn, (long)path, (long)argc, (long)argv));
}

static inline int waitpid(int pid)
{
    int code;
    while ((code = (int)__syscall(SYS_waitpid, (long)pid, 0, 0)) == -2);
    return code;
}

static inline int __readdir(const char *path, unsigned int index, struct vfs_dentry *entry)
{
    return (int)__sysret(__syscall(SYS_readdir, (long)path, (long)index, (long)entry));
}

static inline void clear(void)
{
    __syscall(SYS_clear, 0, 0, 0);
}

static inline void reboot(void)
{
    __syscall(SYS_reboot, 0, 0, 0);
}

#endif
