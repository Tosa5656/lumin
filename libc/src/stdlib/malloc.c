#include <errno.h>

#define POOL_SIZE (64 * 1024)

static char pool[POOL_SIZE];
static unsigned long used;

void *malloc(unsigned long size)
{
    if (size == 0)
        return 0;

    if (size % 8)
        size += 8 - (size % 8);

    if (used + size > POOL_SIZE)
    {
        errno = ENOMEM;
        return 0;
    }

    void *ptr = pool + used;
    used += size;
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

    void *new = malloc(size);
    if (new)
    {
        char *src = (char *)ptr;
        char *dst = (char *)new;
        unsigned long old_size = (unsigned long)((char *)&pool[used] - (char *)ptr);
        unsigned long copy = old_size < size ? old_size : size;
        for (unsigned long i = 0; i < copy; i++)
            dst[i] = src[i];
    }
    return new;
}