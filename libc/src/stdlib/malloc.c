#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stddef.h>

#define ALIGN8(x) (((x) + 7) & ~7)
#define HDR_SZ 8
#define MIN_BLOCK (HDR_SZ + sizeof(void*) + 8)

struct block {
    unsigned long size;
};

static struct block *free_list = NULL;

static struct block *block_from_ptr(void *ptr)
{
    return (struct block*)((char*)ptr - HDR_SZ);
}

static int is_free(struct block *b)
{
    return (int)(b->size & 1);
}

static unsigned long block_real_size(struct block *b)
{
    return b->size & ~1UL;
}

static void **free_next_ptr(struct block *b)
{
    return (void**)((char*)b + HDR_SZ);
}

void *malloc(unsigned long size)
{
    if (size == 0)
        return NULL;

    size = ALIGN8(size);
    unsigned long total = HDR_SZ + size;

    struct block **pp = &free_list;
    while (*pp)
    {
        struct block *b = *pp;
        unsigned long bsize = block_real_size(b);
        if (bsize >= total)
        {
            unsigned long remainder = bsize - total;
            if (remainder >= MIN_BLOCK)
            {
                struct block *nb = (struct block*)((char*)b + total);
                nb->size = remainder | 1;
                *free_next_ptr(nb) = *free_next_ptr(b);
                *pp = nb;
                b->size = total;
            }
            else
            {
                *pp = (struct block*)*free_next_ptr(b);
                b->size = bsize;
            }
            return (char*)b + HDR_SZ;
        }
        pp = (struct block**)free_next_ptr(b);
    }

    struct block *b = (struct block*)sbrk(total);
    if (b == (void*)-1)
    {
        errno = ENOMEM;
        return NULL;
    }
    b->size = total;
    return (char*)b + HDR_SZ;
}

void free(void *ptr)
{
    if (!ptr)
        return;

    struct block *b = block_from_ptr(ptr);
    unsigned long bsize = block_real_size(b);
    b->size = bsize | 1;

    struct block *nb = (struct block*)((char*)b + bsize);
    if ((char*)nb <= (char*)sbrk(0) && is_free(nb) && nb != b)
    {
        bsize += block_real_size(nb);
        *free_next_ptr(b) = *free_next_ptr(nb);
    }
    else
    {
        *free_next_ptr(b) = free_list;
    }

    free_list = b;
}

void *calloc(unsigned long nmemb, unsigned long size)
{
    if (nmemb == 0 || size == 0)
        return NULL;
    if (nmemb > (unsigned long)-1 / size)
    {
        errno = ENOMEM;
        return NULL;
    }
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
    if (!ptr)
        return malloc(size);
    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    size = ALIGN8(size);
    struct block *b = block_from_ptr(ptr);
    unsigned long old_size = block_real_size(b) - HDR_SZ;

    if (size <= old_size)
        return ptr;

    struct block *nb = (struct block*)((char*)b + block_real_size(b));
    if ((char*)nb <= (char*)sbrk(0) && is_free(nb))
    {
        unsigned long combined = block_real_size(b) + block_real_size(nb);
        if (combined >= HDR_SZ + size)
        {
            unsigned long remainder = combined - (HDR_SZ + size);
            if (remainder >= MIN_BLOCK)
            {
                struct block *split = (struct block*)((char*)b + HDR_SZ + size);
                split->size = remainder | 1;
                *free_next_ptr(split) = *free_next_ptr(nb);
                free_list = split;
                b->size = HDR_SZ + size;
            }
            else
            {
                b->size = combined;
                struct block **pp = &free_list;
                while (*pp)
                {
                    if (*pp == nb)
                    {
                        *pp = (struct block*)*free_next_ptr(nb);
                        break;
                    }
                    pp = (struct block**)free_next_ptr(*pp);
                }
            }
            return ptr;
        }
    }

    void *new = malloc(size);
    if (new)
    {
        char *src = (char*)ptr;
        char *dst = (char*)new;
        for (unsigned long i = 0; i < old_size; i++)
            dst[i] = src[i];
        free(ptr);
    }
    return new;
}
