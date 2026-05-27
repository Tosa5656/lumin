#include <unistd.h>

static inline long syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)syscall(0, (long)fd, (long)buf, (long)count);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)syscall(1, (long)fd, (long)buf, (long)count);
}
