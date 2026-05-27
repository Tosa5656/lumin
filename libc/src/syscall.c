long syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}