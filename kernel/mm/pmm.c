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

size_t pmm_free_count(void)
{
    return free_pages;
}