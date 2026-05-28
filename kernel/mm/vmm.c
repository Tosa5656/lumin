#include "vmm.h"
#include "pmm.h"
#include "../drivers/serial/serial.h"

#define PAGE_TABLE_FLAGS (PAGE_PRESENT | PAGE_WRITE)

uint64_t vmm_read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void vmm_write_cr3(uint64_t cr3)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

uint64_t *vmm_create_pml4(void)
{
    uint64_t *kernel_pml4 = (uint64_t *)(uintptr_t)vmm_read_cr3();
    void *page = pmm_alloc();
    if (!page) return NULL;

    uint64_t *new_pml4 = (uint64_t *)page;

    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++)
        new_pml4[i] = kernel_pml4[i];

    for (int i = 1; i < 256; i++)
        new_pml4[i] = 0;

    return new_pml4;
}

static uint64_t *walk_or_create(uint64_t *table, int level, uint64_t virt, uint64_t entry_flags)
{
    int shift = 39 - level * 9;
    int idx = (virt >> shift) & 0x1FF;

    if (table[idx] & PAGE_PRESENT)
    {
        if ((table[idx] & PAGE_HUGE) && level < 3)
            return NULL;
        return (uint64_t *)(uintptr_t)(table[idx] & ~0xFFFULL);
    }

    void *page = pmm_alloc();
    if (!page) return NULL;

    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++)
        ((uint64_t *)page)[i] = 0;

    table[idx] = (uint64_t)(uintptr_t)page | entry_flags | PAGE_PRESENT;
    return (uint64_t *)page;
}

int vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t leaf_flags)
{
    uint64_t table_flags = PAGE_TABLE_FLAGS;
    if (leaf_flags & PAGE_USER)
        table_flags |= PAGE_USER;

    uint64_t *pdpt = walk_or_create(pml4, 0, virt, table_flags);
    if (!pdpt) return -1;
    uint64_t *pd = walk_or_create(pdpt, 1, virt, table_flags);
    if (!pd) return -1;
    uint64_t *pt = walk_or_create(pd, 2, virt, table_flags);
    if (!pt) return -1;

    int pt_idx = (virt >> 12) & 0x1FF;
    if (pt[pt_idx] & PAGE_PRESENT)
        return -1;

    pt[pt_idx] = phys | leaf_flags | PAGE_PRESENT;
    return 0;
}

int vmm_map_range(uint64_t *pml4, uint64_t virt, uint64_t phys, size_t pages, uint64_t flags)
{
    for (size_t i = 0; i < pages; i++)
    {
        if (vmm_map_page(pml4, virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags) != 0)
            return -1;
    }
    return 0;
}

void vmm_load_pml4(uint64_t *pml4)
{
    vmm_write_cr3((uint64_t)(uintptr_t)pml4);
}
