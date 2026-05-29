#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define ALIGN8(x) (((x) + 7) & ~7)

void *malloc(unsigned long size)
{
    if (size == 0)
        return 0;

    size = ALIGN8(size);

    void *ptr = sbrk(size);
    if (ptr == (void *)-1)
    {
        errno = ENOMEM;
        return 0;
    }
    return ptr;
}

void free(void *ptr)
{
    (void)ptr;
}

void *calloc(unsigned long nmemb, unsigned long size)
{
    unsigned long total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr)
    {
        char *p = (char *)ptr;
        for (unsigned long i = 0; i < total; i++)
            p[i] = 0;
    }
    return ptr;
}

void *realloc(void *ptr, unsigned long size)
{
    if (ptr == 0)
        return malloc(size);
    if (size == 0)
    {
        free(ptr);
        return 0;
    }

    size = ALIGN8(size);

    void *new = malloc(size);
    if (new)
    {
        char *src = (char *)ptr;
        char *dst = (char *)new;

        long old_brk = syscall(SYS_brk, 0, 0, 0);
        unsigned long old_size = (unsigned long)((char *)old_brk - (char *)ptr);
        unsigned long copy = old_size < size ? old_size : size;

        for (unsigned long i = 0; i < copy; i++)
            dst[i] = src[i];
    }
    return new;
}
