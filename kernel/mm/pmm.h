#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

void pmm_init(void);
void *pmm_alloc(void);
void pmm_free(void *page);
size_t pmm_free_count(void);

#endif