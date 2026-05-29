#include <stdio.h>
#include <sys/syscall.h>

typedef void (*putchar_t)(char, void *);

static void print_str(const char *s, putchar_t out, void *ctx)
{
    for (; *s; s++)
        out(*s, ctx);
}

static void print_num(unsigned long long n, int base, int upper, putchar_t out, void *ctx)
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
        out(buf[--i], ctx);
}

void vcb_printf(const char *fmt, va_list ap, putchar_t out, void *ctx)
{
    for (; *fmt; fmt++)
    {
        if (*fmt != '%')
        {
            out(*fmt, ctx);
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
                    for (int i = 0; i < pad - len; i++) out(' ', ctx);
                print_str(s, out, ctx);
                if (left && pad > len)
                    for (int i = 0; i < pad - len; i++) out(' ', ctx);
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
                        for (int i = 0; i < pad - tlen; i++) out(pad_char, ctx);
                    if (v < 0) out('-', ctx);
                    while (tlen > 0) out(tmp[--tlen], ctx);
                    if (left && pad > tlen)
                        for (int i = 0; i < pad - tlen; i++) out(' ', ctx);
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
                    for (int i = 0; i < pad - tlen; i++) out(pad_char, ctx);
                while (tlen > 0) out(tmp[--tlen], ctx);
                if (left && pad > tlen)
                    for (int i = 0; i < pad - tlen; i++) out(' ', ctx);
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
                    for (int i = 0; i < pad - tlen; i++) out(pad_char, ctx);
                while (tlen > 0) out(tmp[--tlen], ctx);
                if (left && pad > tlen)
                    for (int i = 0; i < pad - tlen; i++) out(' ', ctx);
                break;
            }
            case 'p':
            {
                void *p = va_arg(ap, void *);
                unsigned long long v = (unsigned long long)p;
                out('0', ctx);
                out('x', ctx);
                if (v == 0)
                    out('0', ctx);
                else
                    print_num(v, 16, 0, out, ctx);
                break;
            }
            case 'c':
            {
                int c = va_arg(ap, int);
                out((char)c, ctx);
                break;
            }
            case '%':
                out('%', ctx);
                break;
            default:
                out('%', ctx);
                out(*fmt, ctx);
                break;
        }
    }
}

struct strbuf {
    char *buf;
    unsigned long pos;
    unsigned long max;
};

static void strbuf_out(char c, void *ctx)
{
    struct strbuf *sb = (struct strbuf *)ctx;
    if (sb->pos < sb->max)
        sb->buf[sb->pos] = c;
    sb->pos++;
}

int vsnprintf(char *buf, unsigned long n, const char *fmt, va_list ap)
{
    struct strbuf sb;
    sb.buf = buf;
    sb.pos = 0;
    sb.max = n > 0 ? n - 1 : 0;

    vcb_printf(fmt, ap, strbuf_out, &sb);

    if (n > 0)
        buf[sb.pos < n ? sb.pos : n - 1] = '\0';
    return (int)sb.pos;
}

int vsprintf(char *buf, const char *fmt, va_list ap)
{
    return vsnprintf(buf, (unsigned long)-1, fmt, ap);
}

static void stdout_putchar(char c, void *ctx)
{
    (void)ctx;
    unsigned char ch = (unsigned char)c;
    syscall(SYS_write, 1, (long)&ch, 1);
}

int putchar(int c)
{
    unsigned char ch = (unsigned char)c;
    syscall(SYS_write, 1, (long)&ch, 1);
    return (unsigned char)c;
}

int puts(const char *s)
{
    for (; *s; s++)
        stdout_putchar(*s, NULL);
    stdout_putchar('\n', NULL);
    return 1;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcb_printf(fmt, ap, stdout_putchar, NULL);
    va_end(ap);
    return 0;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

int snprintf(char *buf, unsigned long n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}
