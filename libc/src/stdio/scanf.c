#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

struct scan_source {
    int (*get)(void *);
    void (*unget)(int, void *);
    void *ctx;
};

static int skip_ws(struct scan_source *src)
{
    int c;
    while ((c = src->get(src->ctx)) != EOF && isspace(c))
        ;
    if (c != EOF)
        src->unget(c, src->ctx);
    return c;
}

static int scan_int(struct scan_source *src, int base, int width, int is_unsigned, void *result, int size)
{
    int c = skip_ws(src);
    if (c == EOF) return 0;

    int neg = 0;
    unsigned long long val = 0;
    int count = 0;

    if (c == '-') { neg = 1; count++; if (width > 0 && count >= width) c = EOF; else c = src->get(src->ctx); }
    else if (c == '+') { count++; if (width > 0 && count >= width) c = EOF; else c = src->get(src->ctx); }

    if (c == EOF) { src->unget(neg ? '-' : '+', src->ctx); return 0; }

    if (base == 0)
    {
        if (c == '0')
        {
            count++;
            if (width > 0 && count >= width) { base = 8; }
            else
            {
                c = src->get(src->ctx);
                if (c == 'x' || c == 'X') { base = 16; c = src->get(src->ctx); count++; }
                else { base = 8; src->unget(c, src->ctx); }
            }
        }
        else
            base = 10;
    }

    int ndigits = 0;
    while ((width == 0 || count < width) && c != EOF)
    {
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;

        if (digit < 0 || digit >= base) break;

        val = val * base + digit;
        ndigits++;
        count++;
        c = src->get(src->ctx);
    }

    if (c != EOF) src->unget(c, src->ctx);
    if (ndigits == 0) return 0;

    if (neg) val = (unsigned long long)(-(long long)val);

    if (size >= 8)
        *(unsigned long long *)result = val;
    else if (size >= 4)
        *(unsigned int *)result = (unsigned int)val;
    else if (size >= 2)
        *(unsigned short *)result = (unsigned short)val;
    else
        *(unsigned char *)result = (unsigned char)val;

    return 1;
}

static int scan_string(struct scan_source *src, int width, void *result, int is_scanlist, const char *scanlist)
{
    int c = skip_ws(src);
    if (c == EOF) return 0;

    char *p = (char *)result;
    int count = 0;

    while (c != EOF && (width == 0 || count < width))
    {
        if (is_scanlist)
        {
            int match = 0;
            int negate = 0;
            const char *sl = scanlist;
            if (*sl == '^') { negate = 1; sl++; }
            while (*sl)
            {
                if (*sl == c) { match = 1; break; }
                sl++;
            }
            if (negate) match = !match;
            if (!match) break;
        }
        else if (isspace(c))
            break;

        *p++ = (char)c;
        count++;
        c = src->get(src->ctx);
    }

    if (c != EOF) src->unget(c, src->ctx);
    *p = '\0';
    return count > 0 ? 1 : 0;
}

static int scan_chars(struct scan_source *src, int width, void *result)
{
    int count = 0;
    char *p = (char *)result;

    while ((width == 0 || count < width))
    {
        int c = src->get(src->ctx);
        if (c == EOF) break;
        *p++ = (char)c;
        count++;
    }

    return count > 0 ? 1 : 0;
}

static int vcb_scanf(struct scan_source *src, const char *fmt, va_list ap)
{
    int items = 0;

    while (*fmt)
    {
        if (isspace((unsigned char)*fmt))
        {
            fmt++;
            continue;
        }

        if (*fmt != '%')
        {
            int c = src->get(src->ctx);
            if (c != (unsigned char)*fmt)
            {
                if (c != EOF) src->unget(c, src->ctx);
                return items;
            }
            fmt++;
            continue;
        }

        fmt++;
        int width = 0;
        int is_long = 0;
        int is_scanlist = 0;
        const char *scanlist = NULL;

        if (*fmt >= '1' && *fmt <= '9')
        {
            width = 0;
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt++ - '0');
        }

        while (*fmt == 'l') { is_long++; fmt++; }
        while (*fmt == 'h') { fmt++; }

        if (*fmt == '[')
        {
            fmt++;
            is_scanlist = 1;
            scanlist = fmt;
            if (*fmt == '^') fmt++;
            if (*fmt == ']') fmt++;
            while (*fmt && *fmt != ']') fmt++;
        }

        switch (*fmt)
        {
            case 'd':
            case 'i':
            {
                void *p = va_arg(ap, void *);
                int sz = is_long >= 2 ? 8 : (is_long >= 1 ? 8 : 4);
                if (scan_int(src, *fmt == 'i' ? 0 : 10, width, 0, p, sz))
                    items++;
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            {
                void *p = va_arg(ap, void *);
                int sz = is_long >= 2 ? 8 : (is_long >= 1 ? 8 : 4);
                int base = (*fmt == 'u') ? 10 : 16;
                if (scan_int(src, base, width, 1, p, sz))
                    items++;
                break;
            }
            case 's':
            {
                char *p = va_arg(ap, char *);
                if (scan_string(src, width, p, 0, NULL))
                    items++;
                break;
            }
            case 'c':
            {
                char *p = va_arg(ap, char *);
                if (scan_chars(src, width ? width : 1, p))
                    items++;
                break;
            }
            case '[':
            {
                char *p = va_arg(ap, char *);
                if (scan_string(src, width, p, 1, scanlist))
                    items++;
                break;
            }
            case 'n':
            {
                int *p = va_arg(ap, int *);
                *p = items;
                break;
            }
            case '%':
            {
                int c = src->get(src->ctx);
                if (c != '%')
                {
                    if (c != EOF) src->unget(c, src->ctx);
                    return items;
                }
                break;
            }
            default:
                break;
        }

        if (*fmt) fmt++;
    }

    return items;
}

static int file_get(void *ctx)
{
    return fgetc((FILE *)ctx);
}

static void file_unget(int c, void *ctx)
{
    if (c != EOF)
        ungetc(c, (FILE *)ctx);
}

struct str_src {
    const char *p;
};

static int str_get(void *ctx)
{
    struct str_src *s = (struct str_src *)ctx;
    if (!*s->p) return EOF;
    return (unsigned char)*s->p++;
}

static void str_unget(int c, void *ctx)
{
    struct str_src *s = (struct str_src *)ctx;
    if (c != EOF && s->p > (const char *)0)
        s->p--;
}

int fscanf(FILE *f, const char *fmt, ...)
{
    struct scan_source src;
    src.get = file_get;
    src.unget = file_unget;
    src.ctx = f;

    va_list ap;
    va_start(ap, fmt);
    int r = vcb_scanf(&src, fmt, ap);
    va_end(ap);
    return r;
}

int scanf(const char *fmt, ...)
{
    struct scan_source src;
    src.get = file_get;
    src.unget = file_unget;
    src.ctx = stdin;

    va_list ap;
    va_start(ap, fmt);
    int r = vcb_scanf(&src, fmt, ap);
    va_end(ap);
    return r;
}

int sscanf(const char *buf, const char *fmt, ...)
{
    struct str_src ss;
    ss.p = buf;

    struct scan_source src;
    src.get = str_get;
    src.unget = str_unget;
    src.ctx = &ss;

    va_list ap;
    va_start(ap, fmt);
    int r = vcb_scanf(&src, fmt, ap);
    va_end(ap);
    return r;
}
