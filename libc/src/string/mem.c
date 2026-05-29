void *memset(void *dst, int c, unsigned long n)
{
    unsigned long d = (unsigned long)dst;
    unsigned long r = d & 7;
    unsigned long v = (unsigned char)c;
    v |= v << 8;
    v |= v << 16;
    v |= v << 32;

    if (r)
    {
        unsigned long part = 8 - r;
        if (part > n)
            part = n;
        for (unsigned long i = 0; i < part; i++)
            ((char*)d)[i] = (char)c;
        d += part;
        n -= part;
    }

    unsigned long *dp = (unsigned long*)d;
    while (n >= 8)
    {
        *dp++ = v;
        n -= 8;
    }

    char *cp = (char*)dp;
    for (unsigned long i = 0; i < n; i++)
        cp[i] = (char)c;

    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
    char *d = (char*)dst;
    const char *s = (const char*)src;

    if (((unsigned long)d & 7) == 0 && ((unsigned long)s & 7) == 0)
    {
        unsigned long *ld = (unsigned long*)d;
        const unsigned long *ls = (const unsigned long*)s;
        while (n >= 8)
        {
            *ld++ = *ls++;
            n -= 8;
        }
        d = (char*)ld;
        s = (const char*)ls;
    }

    for (unsigned long i = 0; i < n; i++)
        d[i] = s[i];

    return dst;
}

void *memmove(void *dst, const void *src, unsigned long n)
{
    char *d = (char*)dst;
    const char *s = (const char*)src;

    if (d < s)
    {
        for (unsigned long i = 0; i < n; i++)
            d[i] = s[i];
    }
    else
    {
        for (unsigned long i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return dst;
}

int memcmp(const void *a, const void *b, unsigned long n)
{
    const char *ca = (const char*)a;
    const char *cb = (const char*)b;
    for (unsigned long i = 0; i < n; i++)
    {
        if (ca[i] != cb[i])
            return (unsigned char)ca[i] - (unsigned char)cb[i];
    }
    return 0;
}

void *memchr(const void *s, int c, unsigned long n)
{
    const char *p = (const char*)s;
    for (unsigned long i = 0; i < n; i++)
    {
        if (p[i] == (char)c)
            return (void*)(p + i);
    }
    return 0;
}
