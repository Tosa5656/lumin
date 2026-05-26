#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 0x1000
#define PAGE_TABLE_ENTRIES 512

#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_WRITE     (1ULL << 1)
#define PAGE_USER      (1ULL << 2)
#define PAGE_HUGE      (1ULL << 7)
#define PAGE_BIG       (1ULL << 7)

#define USER_BASE_VADDR 0x8000000000ULL

uint64_t vmm_read_cr3(void);
void     vmm_write_cr3(uint64_t cr3);

uint64_t *vmm_create_pml4(void);
int       vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
int       vmm_map_range(uint64_t *pml4, uint64_t virt, uint64_t phys, size_t pages, uint64_t flags);
void      vmm_load_pml4(uint64_t *pml4);

#endif
