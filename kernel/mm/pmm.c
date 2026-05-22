#include "pmm.h"

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list = NULL;
static size_t free_pages = 0;

void pmm_init(uint64_t start, uint64_t end)
{
    free_list = NULL;
    free_pages = 0;
    start = (start + 0xFFF) & ~0xFFFULL;
    end   = end & ~0xFFFULL;
    for (uint64_t addr = start; addr < end; addr += 0x1000)
        pmm_free((void *)addr);
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