#include "pmm.h"
#include "e820.h"
#include "../include/spinlock.h"

static const uint64_t MIN_ADDR = 0x200000;
static const uint64_t DMA_ZONE_LIMIT = 0x1000000;
static const uint64_t DMA32_ZONE_LIMIT = 0x100000000ULL;

struct free_page {
    struct free_page *next;
};

static struct free_page *dma_free_list = NULL;
static struct free_page *dma32_free_list = NULL;
static struct free_page *normal_free_list = NULL;
static size_t free_pages_total = 0;
static spinlock_t pmm_lock = SPINLOCK_INIT;

static void page_free_internal(uint64_t addr)
{
    struct free_page *fp = (struct free_page *)(uintptr_t)addr;
    if (addr < DMA32_ZONE_LIMIT)
    {
        if (addr < DMA_ZONE_LIMIT)
        {
            fp->next = dma_free_list;
            dma_free_list = fp;
        }
        else
        {
            fp->next = dma32_free_list;
            dma32_free_list = fp;
        }
    }
    else
    {
        fp->next = normal_free_list;
        normal_free_list = fp;
    }
    free_pages_total++;
}

static void add_range(uint64_t start, uint64_t end)
{
    start = (start + 0xFFF) & ~0xFFFULL;
    end   = end & ~0xFFFULL;
    for (uint64_t addr = start; addr < end; addr += 0x1000)
        page_free_internal(addr);
}

static void *zone_alloc(struct free_page **list)
{
    if (!*list) return NULL;
    struct free_page *page = *list;
    *list = page->next;
    free_pages_total--;
    return page;
}

void pmm_init(void)
{
    dma_free_list = NULL;
    dma32_free_list = NULL;
    normal_free_list = NULL;
    free_pages_total = 0;

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

    if (free_pages_total == 0)
        add_range(MIN_ADDR, 0x400000);
}

void *pmm_alloc(void)
{
    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);
    void *p = zone_alloc(&normal_free_list);
    if (!p) p = zone_alloc(&dma32_free_list);
    if (!p) p = zone_alloc(&dma_free_list);
    spin_unlock_irqrestore(&pmm_lock, flags);
    return p;
}

void *pmm_alloc_dma(void)
{
    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);
    void *p = zone_alloc(&dma_free_list);
    spin_unlock_irqrestore(&pmm_lock, flags);
    return p;
}

void *pmm_alloc_below(uint64_t max_addr)
{
    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);
    void *p = NULL;
    if (max_addr <= DMA_ZONE_LIMIT)
        p = zone_alloc(&dma_free_list);
    else if (max_addr <= DMA32_ZONE_LIMIT)
    {
        p = zone_alloc(&dma_free_list);
        if (!p) p = zone_alloc(&dma32_free_list);
    }
    else
        p = pmm_alloc();
    spin_unlock_irqrestore(&pmm_lock, flags);
    return p;
}

void pmm_free(void *page)
{
    if (!page) return;
    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);
    page_free_internal((uint64_t)(uintptr_t)page);
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void *pmm_alloc_pages(size_t n)
{
    if (n == 0) return NULL;

    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);

    uint64_t highest = (uint64_t)(uintptr_t)zone_alloc(&normal_free_list);
    if (!highest)
    {
        highest = (uint64_t)(uintptr_t)zone_alloc(&dma32_free_list);
        if (!highest)
        {
            highest = (uint64_t)(uintptr_t)zone_alloc(&dma_free_list);
            if (!highest)
            {
                spin_unlock_irqrestore(&pmm_lock, flags);
                return NULL;
            }
        }
    }

    free_pages_total -= (n - 1);

    for (size_t i = 1; i < n; i++)
    {
        void *page = zone_alloc(&normal_free_list);
        if (!page)
        {
            page = zone_alloc(&dma32_free_list);
            if (!page)
            {
                page = zone_alloc(&dma_free_list);
                if (!page)
                {
                    for (size_t j = 0; j < i; j++)
                        page_free_internal(highest - j * 0x1000);
                    free_pages_total += (n - 1);
                    spin_unlock_irqrestore(&pmm_lock, flags);
                    return NULL;
                }
            }
        }
    }

    spin_unlock_irqrestore(&pmm_lock, flags);
    return (void *)(uintptr_t)(highest - (n - 1) * 0x1000);
}

void pmm_free_pages(void *addr, size_t n)
{
    if (!addr) return;
    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);
    for (size_t i = 0; i < n; i++)
        page_free_internal((uint64_t)(uintptr_t)addr + i * 0x1000);
    spin_unlock_irqrestore(&pmm_lock, flags);
}

size_t pmm_free_count(void)
{
    unsigned long flags;
    spin_lock_irqsave(&pmm_lock, &flags);
    size_t n = free_pages_total;
    spin_unlock_irqrestore(&pmm_lock, flags);
    return n;
}