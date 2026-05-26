static inline long syscall(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x30" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}

static void _exit(int code)
{
    syscall(60, code, 0, 0);
    while (1) {}
}

static int strlen(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void)
{
    const char *m1 = "Hello from exec'ed program!\n";
    syscall(1, 1, (long)m1, strlen(m1));

    const char *m2 = "Userspace exec works!\n";
    syscall(1, 1, (long)m2, strlen(m2));

    _exit(0);
}
