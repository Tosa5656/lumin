#include "kmalloc.h"
#include "pmm.h"
#include "drivers/serial/serial.h"

#define HEAP_ALIGN 16
#define HEAP_MAGIC 0xDEADBEEF

struct heap_block {
    uint32_t magic;
    uint32_t size;
    struct heap_block *next;
    uint8_t free;
};

static struct heap_block *heap_root = NULL;

static void heap_add_pages(void *addr)
{
    struct heap_block *block = (struct heap_block *)addr;
    block->magic = HEAP_MAGIC;
    block->size  = 0x1000;
    block->next  = NULL;
    block->free  = 1;

    if (!heap_root) {
        heap_root = block;
        return;
    }

    struct heap_block *cur = heap_root;
    while (cur->next)
        cur = cur->next;
    cur->next = block;
}

void kmalloc_init(void)
{
    for (int i = 0; i < 64; i++)
    {
        void *p = pmm_alloc();
        if (!p) {
            serial_write("kmalloc_init: pmm_alloc failed\n");
            return;
        }
        heap_add_pages(p);
    }
    serial_write("kmalloc: 64 pages added to heap\n");
}

static void heap_coalesce(void)
{
    struct heap_block *cur = heap_root;
    while (cur && cur->next)
    {
        if (cur->free && cur->next->free)
        {
            cur->size += cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }
}

void *kmalloc(uint64_t size)
{
    if (size == 0) return NULL;

    uint64_t needed = size + sizeof(struct heap_block);
    if (needed < 16) needed = 16;
    needed = (needed + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);

    heap_coalesce();

    struct heap_block *cur = heap_root;
    while (cur) {
        if (cur->free && cur->size >= needed) {
            if (cur->size >= needed + sizeof(struct heap_block) + 16) {
                struct heap_block *new_block = (struct heap_block *)((uint8_t *)cur + needed);
                new_block->magic = HEAP_MAGIC;
                new_block->size  = cur->size - needed;
                new_block->next  = cur->next;
                new_block->free  = 1;
                cur->next = new_block;
                cur->size = needed;
            }
            cur->free = 0;
            return (uint8_t *)cur + sizeof(struct heap_block);
        }
        cur = cur->next;
    }

    void *page = pmm_alloc();
    if (!page) return NULL;
    heap_add_pages(page);

    heap_coalesce();

    cur = heap_root;
    while (cur) {
        if (cur->free && cur->size >= needed) {
            if (cur->size >= needed + sizeof(struct heap_block) + 16) {
                struct heap_block *new_block = (struct heap_block *)((uint8_t *)cur + needed);
                new_block->magic = HEAP_MAGIC;
                new_block->size  = cur->size - needed;
                new_block->next  = cur->next;
                new_block->free  = 1;
                cur->next = new_block;
                cur->size = needed;
            }
            cur->free = 0;
            return (uint8_t *)cur + sizeof(struct heap_block);
        }
        cur = cur->next;
    }
    return NULL;
}

void kfree(void *ptr)
{
    if (!ptr) return;
    struct heap_block *block = (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
    if (block->magic != HEAP_MAGIC) {
        serial_write("kfree: invalid magic\n");
        return;
    }
    block->free = 1;

    struct heap_block *cur = heap_root;
    while (cur) {
        if (cur->free && cur->next && cur->next->free) {
            cur->size += cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }

    cur = heap_root;
    while (cur) {
        if (cur->free && cur->size == 0x1000 && !cur->next)
        {
            if (cur == heap_root)
                heap_root = NULL;
            else
            {
                struct heap_block *prev = heap_root;
                while (prev && prev->next != cur)
                    prev = prev->next;
                if (prev)
                    prev->next = NULL;
            }
            pmm_free(cur);
            return;
        }

        if (cur->free && cur->size == 0x1000 && cur->next && cur->next->free)
        {
            cur->size += cur->next->size;
            cur->next = cur->next->next;
        }

        if (cur->free && cur->size >= 0x2000 && (!cur->next || !cur->next->free))
        {
            uint64_t extra = (cur->size - 0x1000) & ~0xFFFULL;
            if (extra >= 0x1000)
            {
                cur->size -= extra;
                struct heap_block *freed = (struct heap_block *)((uint8_t *)cur + cur->size);
                pmm_free(freed);
            }
        }

        cur = cur->next;
    }
}