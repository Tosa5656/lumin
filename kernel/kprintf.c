#include "kprintf.h"

static void print_dec(kputch_t putch, unsigned long long n, int sign, int width, int pad)
{
    if (sign && (long long)n < 0)
    {
        putch('-');
        n = -(long long)n;
        width--;
        if (width < 0) width = 0;
    }
    char buf[21];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
    while (i < width) buf[i++] = pad ? '0' : ' ';
    while (i > 0) putch(buf[--i]);
}

static void print_hex(kputch_t putch, unsigned long long n, int width, int pad, int upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[17];
    int i = 0;
    if (n == 0) buf[i++] = '0';
    while (n > 0) { buf[i++] = digits[n & 0xF]; n >>= 4; }
    while (i < width) buf[i++] = pad ? '0' : ' ';
    while (i > 0) putch(buf[--i]);
}

void kvformat(kputch_t putch, const char *fmt, va_list ap)
{
    while (*fmt)
    {
        if (*fmt != '%')
        {
            putch(*fmt++);
            continue;
        }
        fmt++;

        int pad = 0;
        int width = 0;
        if (*fmt == '0') { pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        switch (*fmt)
        {
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) putch(*s++);
                break;
            }
            case 'd':
                print_dec(putch, va_arg(ap, int), 1, width, pad);
                break;
            case 'u':
                print_dec(putch, va_arg(ap, unsigned int), 0, width, pad);
                break;
            case 'x':
                print_hex(putch, va_arg(ap, unsigned int), width, pad, 0);
                break;
            case 'X':
                print_hex(putch, va_arg(ap, unsigned int), width, pad, 1);
                break;
            case 'p':
            {
                putch('0'); putch('x');
                unsigned long long v = (unsigned long long)va_arg(ap, void *);
                int started = 0;
                for (int i = 15; i >= 0; i--)
                {
                    int d = (v >> (i * 4)) & 0xF;
                    if (d || started || i == 0) { started = 1; putch("0123456789abcdef"[d]); }
                }
                break;
            }
            case 'c':
                putch((char)va_arg(ap, int));
                break;
            case '%':
                putch('%');
                break;
        }
        fmt++;
    }
}
