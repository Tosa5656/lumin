#include <sys/syscall.h>

void exit(int code)
{
    syscall(SYS_exit, code, 0, 0);
    while (1) {}
}

void abort(void)
{
    exit(-1);
}

int atoi(const char *s)
{
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return sign * n;
}