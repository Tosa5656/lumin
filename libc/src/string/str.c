#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

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

int strcasecmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 0x20 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 0x20 : *b;
        if (ca != cb)
            return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncasecmp(const char *a, const char *b, unsigned long n)
{
    for (unsigned long i = 0; i < n; i++)
    {
        if (a[i] == '\0' && b[i] == '\0') return 0;
        char ca = (a[i] >= 'A' && a[i] <= 'Z') ? a[i] + 0x20 : a[i];
        char cb = (b[i] >= 'A' && b[i] <= 'Z') ? b[i] + 0x20 : b[i];
        if (ca != cb)
            return (unsigned char)ca - (unsigned char)cb;
        if (a[i] == '\0') break;
    }
    return 0;
}

char *strpbrk(const char *s, const char *accept)
{
    while (*s)
    {
        const char *a = accept;
        while (*a)
        {
            if (*s == *a)
                return (char *)s;
            a++;
        }
        s++;
    }
    return 0;
}

unsigned long strspn(const char *s, const char *accept)
{
    unsigned long n = 0;
    while (s[n])
    {
        const char *a = accept;
        int found = 0;
        while (*a)
        {
            if (s[n] == *a) { found = 1; break; }
            a++;
        }
        if (!found) break;
        n++;
    }
    return n;
}

unsigned long strcspn(const char *s, const char *reject)
{
    unsigned long n = 0;
    while (s[n])
    {
        const char *r = reject;
        while (*r)
        {
            if (s[n] == *r) return n;
            r++;
        }
        n++;
    }
    return n;
}

char *strncat(char *dst, const char *src, unsigned long n)
{
    char *d = dst + strlen(dst);
    unsigned long i;
    for (i = 0; i < n && src[i]; i++)
        d[i] = src[i];
    d[i] = '\0';
    return dst;
}

char *strerror(int errnum)
{
    switch (errnum)
    {
        case 0:      return "Success";
        case EPERM:  return "Operation not permitted";
        case ENOENT: return "No such file or directory";
        case ESRCH:  return "No such process";
        case EINTR:  return "Interrupted system call";
        case EIO:    return "Input/output error";
        case ENXIO:  return "No such device or address";
        case E2BIG:  return "Argument list too long";
        case ENOEXEC: return "Exec format error";
        case EBADF:  return "Bad file descriptor";
        case ECHILD: return "No child processes";
        case EAGAIN: return "Resource temporarily unavailable";
        case ENOMEM: return "Cannot allocate memory";
        case EACCES: return "Permission denied";
        case EFAULT: return "Bad address";
        case EBUSY:  return "Device or resource busy";
        case EEXIST: return "File exists";
        case EXDEV:  return "Invalid cross-device link";
        case ENODEV: return "No such device";
        case ENOTDIR: return "Not a directory";
        case EISDIR: return "Is a directory";
        case EINVAL: return "Invalid argument";
        case ENFILE: return "File table overflow";
        case EMFILE: return "Too many open files";
        case ENOTTY: return "Not a tty";
        case EFBIG:  return "File too large";
        case ENOSPC: return "No space left on device";
        case ESPIPE: return "Illegal seek";
        case EROFS:  return "Read-only file system";
        case EPIPE:  return "Broken pipe";
        case EDOM:   return "Numerical argument out of domain";
        case ERANGE: return "Numerical result out of range";
        case ENOSYS: return "Function not implemented";
        case ELOOP:  return "Too many levels of symbolic links";
        default:     return "Unknown error";
    }
}
