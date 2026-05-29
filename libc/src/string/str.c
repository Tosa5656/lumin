#include <string.h>
#include <stdlib.h>
#include <stddef.h>

unsigned long strlen(const char *s)
{
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, unsigned long n)
{
    for (unsigned long i = 0; i < n; i++)
    {
        if (a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0')
            break;
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, unsigned long n)
{
    unsigned long i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    if (c == '\0')
        return (char *)s;
    return 0;
}

char *strrchr(const char *s, int c)
{
    const char *last = 0;
    while (*s)
    {
        if (*s == (char)c)
            last = s;
        s++;
    }
    if (c == '\0')
        return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (!*needle)
        return (char *)haystack;
    for (; *haystack; haystack++)
    {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }
        if (!*n)
            return (char *)haystack;
    }
    return 0;
}

char *strdup(const char *s)
{
    unsigned long len = strlen(s);
    char *p = (char *)malloc(len + 1);
    if (p)
    {
        for (unsigned long i = 0; i <= len; i++)
            p[i] = s[i];
    }
    return p;
}

char *strtok(char *str, const char *delim)
{
    static char *save = NULL;
    return strtok_r(str, delim, &save);
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    if (!str)
        str = *saveptr;
    if (!str)
        return NULL;

    while (*str)
    {
        const char *d = delim;
        int is_delim = 0;
        while (*d)
        {
            if (*str == *d)
            {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (!is_delim)
            break;
        str++;
    }

    if (!*str)
    {
        *saveptr = NULL;
        return NULL;
    }

    char *start = str;
    while (*str)
    {
        const char *d = delim;
        int is_delim = 0;
        while (*d)
        {
            if (*str == *d)
            {
                is_delim = 1;
                break;
            }
            d++;
        }
        if (is_delim)
        {
            *str = '\0';
            *saveptr = str + 1;
            return start;
        }
        str++;
    }

    *saveptr = NULL;
    return start;
}
