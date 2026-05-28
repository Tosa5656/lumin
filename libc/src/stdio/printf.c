#include <sys/syscall.h>
#include <stdarg.h>

static void print_str(const char *s, void (*out)(char))
{
    for (; *s; s++)
        out(*s);
}

static void print_num(unsigned long long n, int base, int upper, void (*out)(char))
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (n == 0)
        buf[i++] = '0';
    while (n > 0)
    {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0)
        out(buf[--i]);
}

static void print_num_signed(long long n, int base, int upper, void (*out)(char))
{
    if (n < 0)
    {
        out('-');
        n = -n;
    }
    print_num((unsigned long long)n, base, upper, out);
}

static void vcb_printf(const char *fmt, va_list ap, void (*out)(char))
{
    for (; *fmt; fmt++)
    {
        if (*fmt != '%')
        {
            out(*fmt);
            continue;
        }
        fmt++;
        int pad = 0;
        int left = 0;
        char pad_char = ' ';
        if (*fmt == '-') { left = 1; fmt++; }
        if (*fmt == '0') { pad_char = '0'; fmt++; }
        if (*fmt >= '1' && *fmt <= '9')
        {
            pad = 0;
            while (*fmt >= '0' && *fmt <= '9')
                pad = pad * 10 + (*fmt++ - '0');
        }
        int is_long = 0;
        while (*fmt == 'l') { is_long = 1; fmt++; }

        switch (*fmt)
        {
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                int len = 0;
                while (s[len]) len++;
                if (!left && pad > len)
                    for (int i = 0; i < pad - len; i++) out(' ');
                print_str(s, out);
                if (left && pad > len)
                    for (int i = 0; i < pad - len; i++) out(' ');
                break;
            }
            case 'd':
            case 'i':
            {
                long long v;
                if (is_long) v = va_arg(ap, long long);
                else v = va_arg(ap, int);
                {
                    char tmp[32];
                    int tlen = 0;
                    long long tv = v;
                    if (tv < 0) { tv = -tv; tlen = 1; }
                    unsigned long long u = (unsigned long long)tv;
                    do { tmp[tlen++] = '0' + u % 10; u /= 10; } while (u);
                    if (!left && pad > tlen)
                        for (int i = 0; i < pad - tlen; i++) out(pad_char);
                    if (v < 0) out('-');
                    while (tlen > 0) out(tmp[--tlen]);
                    if (left && pad > tlen)
                        for (int i = 0; i < pad - tlen; i++) out(' ');
                }
                break;
            }
            case 'u':
            {
                unsigned long long v;
                if (is_long) v = va_arg(ap, unsigned long long);
                else v = va_arg(ap, unsigned int);
                char tmp[32];
                int tlen = 0;
                do { tmp[tlen++] = '0' + v % 10; v /= 10; } while (v);
                if (!left && pad > tlen)
                    for (int i = 0; i < pad - tlen; i++) out(pad_char);
                while (tlen > 0) out(tmp[--tlen]);
                if (left && pad > tlen)
                    for (int i = 0; i < pad - tlen; i++) out(' ');
                break;
            }
            case 'x':
            case 'X':
            {
                unsigned long long v;
                if (is_long) v = va_arg(ap, unsigned long long);
                else v = va_arg(ap, unsigned int);
                int upper = (*fmt == 'X');
                char tmp[32];
                int tlen = 0;
                const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
                do { tmp[tlen++] = digits[v % 16]; v /= 16; } while (v);
                if (!left && pad > tlen)
                    for (int i = 0; i < pad - tlen; i++) out(pad_char);
                while (tlen > 0) out(tmp[--tlen]);
                if (left && pad > tlen)
                    for (int i = 0; i < pad - tlen; i++) out(' ');
                break;
            }
            case 'p':
            {
                void *p = va_arg(ap, void *);
                unsigned long long v = (unsigned long long)p;
                out('0');
                out('x');
                if (v == 0)
                    out('0');
                else
                    print_num(v, 16, 0, out);
                break;
            }
            case 'c':
            {
                int c = va_arg(ap, int);
                out((char)c);
                break;
            }
            case '%':
                out('%');
                break;
            default:
                out('%');
                out(*fmt);
                break;
        }
    }
}

static void stdout_putchar(char c)
{
    syscall(SYS_write, 1, (long)&c, 1);
}

int putchar(int c)
{
    char ch = (char)c;
    syscall(SYS_write, 1, (long)&ch, 1);
    return (unsigned char)ch;
}

int puts(const char *s)
{
    print_str(s, stdout_putchar);
    stdout_putchar('\n');
    return 1;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcb_printf(fmt, ap, stdout_putchar);
    va_end(ap);
    return 0;
}