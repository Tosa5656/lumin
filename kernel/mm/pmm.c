#include "pmm.h"
#include "e820.h"

static const uint64_t MIN_ADDR = 0x200000;

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list = NULL;
static size_t free_pages = 0;

static void add_range(uint64_t start, uint64_t end)
{
    start = (start + 0xFFF) & ~0xFFFULL;
    end   = end & ~0xFFFULL;
    for (uint64_t addr = start; addr < end; addr += 0x1000)
        pmm_free((void *)addr);
}

void pmm_init(void)
{
    free_list = NULL;
    free_pages = 0;

    uint16_t count = *E820_COUNT_ADDR;
    if (count == 0 || count > 100)
    {
        add_range(MIN_ADDR, 0x400000);
        return;
    }

    for (uint16_t i = 0; i < count; i++)
    {
        struct e820_entry *e = &E820_ENTRIES[i];
        if (e->type != E820_AVAILABLE)
            continue;

        uint64_t start = e->base;
        uint64_t end   = e->base + e->length;

        if (end <= MIN_ADDR)
            continue;
        if (start < MIN_ADDR)
            start = MIN_ADDR;

        add_range(start, end);
    }

    if (free_pages == 0)
        add_range(MIN_ADDR, 0x400000);
}

void *pmm_alloc(void)
{
    if (!free_list) return NULL;
    struct free_page *page = free_list;
    free_list = page->next;
    free_pages--;
    return page;
}

void pmm_free(void *page)
{
    if (!page) return;
    struct free_page *fp = (struct free_page *)page;
    fp->next = free_list;
    free_list = fp;
    free_pages++;
}

void *pmm_alloc_pages(size_t n)
{
    if (!free_list || n == 0) return NULL;

    uint64_t highest = (uint64_t)pmm_alloc();
    if (!highest) return NULL;

    for (size_t i = 1; i < n; i++)
    {
        void *page = pmm_alloc();
        if (!page)
        {
            for (size_t j = 0; j < i; j++)
                pmm_free((void *)(highest - j * 0x1000));
            return NULL;
        }
    }

    return (void *)(highest - (n - 1) * 0x1000);
}

void pmm_free_pages(void *addr, size_t n)
{
    for (size_t i = 0; i < n; i++)
        pmm_free((void *)((uint64_t)addr + i * 0x1000));
}

size_t pmm_free_count(void)
{
    return free_pages;
}