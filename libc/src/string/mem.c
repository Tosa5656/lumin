void *memset(void *dst, int c, unsigned long n)
{
    char *d = (char *)dst;
    for (unsigned long i = 0; i < n; i++)
        d[i] = (char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (unsigned long i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, unsigned long n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
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
    const char *ca = (const char *)a;
    const char *cb = (const char *)b;
    for (unsigned long i = 0; i < n; i++)
    {
        if (ca[i] != cb[i])
            return (unsigned char)ca[i] - (unsigned char)cb[i];
    }
    return 0;
}