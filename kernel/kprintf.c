#include "kprintf.h"

static void print_dec(kputch_t putch, void *ctx, unsigned long long n, int sign, int width, int pad)
{
    if (sign && (long long)n < 0)
    {
        putch('-', ctx);
        n = -(long long)n;
        width--;
        if (width < 0) width = 0;
    }
    char buf[21];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
    while (i < width) buf[i++] = pad ? '0' : ' ';
    while (i > 0) putch(buf[--i], ctx);
}

static void print_hex(kputch_t putch, void *ctx, unsigned long long n, int width, int pad, int upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[17];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    while (n > 0) { buf[i++] = digits[n & 0xF]; n >>= 4; }
    while (i < width) buf[i++] = pad ? '0' : ' ';
    while (i > 0) putch(buf[--i], ctx);
}

void kvformat(kputch_t putch, void *ctx, const char *fmt, va_list ap)
{
    while (*fmt)
    {
        if (*fmt != '%')
        {
            putch(*fmt++, ctx);
            continue;
        }
        fmt++;

        int pad = 0;
        int width = 0;
        if (*fmt == '0') { pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        int longlong = 0;
        while (*fmt == 'l')
        {
            longlong++;
            fmt++;
        }

        switch (*fmt)
        {
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) putch(*s++, ctx);
                break;
            }
            case 'd':
                if (longlong)
                    print_dec(putch, ctx, va_arg(ap, long long), 1, width, pad);
                else
                    print_dec(putch, ctx, va_arg(ap, int), 1, width, pad);
                break;
            case 'u':
                if (longlong)
                    print_dec(putch, ctx, va_arg(ap, unsigned long long), 0, width, pad);
                else
                    print_dec(putch, ctx, va_arg(ap, unsigned int), 0, width, pad);
                break;
            case 'x':
            case 'X':
            {
                int upper = (*fmt == 'X');
                if (longlong)
                    print_hex(putch, ctx, va_arg(ap, unsigned long long), width, pad, upper);
                else
                    print_hex(putch, ctx, va_arg(ap, unsigned int), width, pad, upper);
                break;
            }
            case 'p':
            {
                putch('0', ctx); putch('x', ctx);
                unsigned long long v = (unsigned long long)va_arg(ap, void *);
                int started = 0;
                for (int i = 15; i >= 0; i--)
                {
                    int d = (v >> (i * 4)) & 0xF;
                    if (d || started || i == 0) { started = 1; putch("0123456789abcdef"[d], ctx); }
                }
                break;
            }
            case 'c':
                putch((char)va_arg(ap, int), ctx);
                break;
            case '%':
                putch('%', ctx);
                break;
        }
        fmt++;
    }
}

struct sbuf {
    char *buf;
    unsigned long pos;
    unsigned long size;
};

static void sbuf_putch(char c, void *ctx)
{
    struct sbuf *s = ctx;
    if (s->pos + 1 < s->size)
        s->buf[s->pos++] = c;
}

int ksnprintf(char *buf, unsigned long size, const char *fmt, ...)
{
    if (!buf || !size) return 0;
    struct sbuf s;
    s.buf = buf;
    s.pos = 0;
    s.size = size;

    va_list ap;
    va_start(ap, fmt);
    kvformat(sbuf_putch, &s, fmt, ap);
    va_end(ap);

    buf[s.pos] = '\0';
    return s.pos;
}