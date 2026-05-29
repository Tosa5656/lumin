#include <sys/syscall.h>
#include <stddef.h>

#define ATEXIT_MAX 32

static void (*atexit_handlers[ATEXIT_MAX])(void);
static int atexit_count = 0;

int atexit(void (*func)(void))
{
    if (atexit_count >= ATEXIT_MAX)
        return -1;
    atexit_handlers[atexit_count++] = func;
    return 0;
}

void exit(int code)
{
    for (int i = atexit_count - 1; i >= 0; i--)
    {
        if (atexit_handlers[i])
            atexit_handlers[i]();
    }
    __syscall(SYS_exit, code, 0, 0);
    while (1) {}
}

void _Exit(int code)
{
    __syscall(SYS_exit, code, 0, 0);
    while (1) {}
}

void abort(void)
{
    _Exit(-1);
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
