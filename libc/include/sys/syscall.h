#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#define SYS_read    0
#define SYS_write   1
#define SYS_exit    60

static inline long syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}

#endif