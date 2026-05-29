#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

long strtol(const char *s, char **endptr, int base)
{
    const char *p = s;
    while (isspace((unsigned char)*p)) p++;

    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') p++;

    if ((base == 0 || base == 16) && *p == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    else if (base == 0 && *p == '0')
    {
        base = 8;
        p++;
    }
    else if (base == 0)
    {
        base = 10;
    }

    long val = 0;
    int any = 0;

    for (;;)
    {
        int c = (unsigned char)*p;
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;

        if (digit >= base) break;

        if (any < 0 || val > (LONG_MAX - digit) / base)
        {
            any = -1;
        }
        else
        {
            val = val * base + digit;
            any = 1;
        }
        p++;
    }

    if (any < 0)
    {
        val = (sign > 0) ? LONG_MAX : LONG_MIN;
        errno = ERANGE;
    }
    else if (any == 0)
    {
        val = 0;
    }
    else
    {
        val *= sign;
    }

    if (endptr)
    {
        *endptr = (char *)(any ? p : s);
    }

    return val;
}

unsigned long strtoul(const char *s, char **endptr, int base)
{
    const char *p = s;
    while (isspace((unsigned char)*p)) p++;

    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;

    if ((base == 0 || base == 16) && *p == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    else if (base == 0 && *p == '0')
    {
        base = 8;
        p++;
    }
    else if (base == 0)
    {
        base = 10;
    }

    unsigned long val = 0;
    int any = 0;

    for (;;)
    {
        int c = (unsigned char)*p;
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;

        if (digit >= base) break;

        if (val > (ULONG_MAX - digit) / base)
        {
            any = -1;
        }
        else
        {
            val = val * base + digit;
            any = 1;
        }
        p++;
    }

    if (any < 0)
    {
        val = ULONG_MAX;
        errno = ERANGE;
    }
    else if (any == 0)
    {
        val = 0;
    }

    if (neg && any > 0)
        val = -val;

    if (endptr)
        *endptr = (char *)(any ? p : s);

    return val;
}
