#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>
#include <stddef.h>
#include "../mm/pmm.h"

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)
#define PAGE_HUGE    (1ULL << 7)

static inline void mmio_map_2mb(uint64_t phys_addr)
{
    uint64_t aligned = phys_addr & ~0x1FFFFFULL;
    uint64_t entry   = aligned | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE;

    volatile uint64_t *pdp = (volatile uint64_t *)0x2000;
    volatile uint64_t *pml4 = (volatile uint64_t *)0x1000;

    int pdp_idx = (int)((aligned >> 30) & 0x1FF);
    int pd_idx  = (int)((aligned >> 21) & 0x1FF);

    if (!(pdp[pdp_idx] & PAGE_PRESENT))
    {
        uint64_t pd_page = (uint64_t)pmm_alloc();

        volatile uint64_t *pd = (volatile uint64_t *)pd_page;
        for (int i = 0; i < 512; i++)
            pd[i] = 0;

        pdp[pdp_idx] = pd_page | PAGE_PRESENT | PAGE_WRITE;

        __asm__ volatile("invlpg (%0)" : : "r"(aligned) : "memory");
    }

    int pdp_idx_check = (int)((aligned >> 30) & 0x1FF);
    (void)pdp_idx_check;

    volatile uint64_t *pd2 = (volatile uint64_t *)(pdp[pdp_idx] & ~0xFFFULL);
    pd2[pd_idx] = entry;

    __asm__ volatile("invlpg (%0)" : : "r"(aligned) : "memory");
}

#endif