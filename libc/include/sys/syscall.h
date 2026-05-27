#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#define SYS_read    0
#define SYS_write   1
#define SYS_brk     45
#define SYS_exit    60

static inline long syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
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

#endif