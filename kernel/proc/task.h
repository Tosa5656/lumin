#ifndef TASK_H
#define TASK_H

#include <stdint.h>

extern uint64_t saved_kernel_rsp;
extern uint64_t saved_kernel_cr3;

extern uint64_t *current_pml4;
extern uint64_t  current_brk;

#define USER_HEAP_START (USER_BASE_VADDR + 0x100000000ULL)

int  task_init(void);
void exec_user(uint64_t entry, uint64_t *pml4);

#endif
