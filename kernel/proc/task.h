#ifndef TASK_H
#define TASK_H

#include <stdint.h>

extern uint64_t saved_kernel_rsp;
extern uint64_t saved_kernel_cr3;

int  task_init(void);
void exec_user(uint64_t entry, uint64_t *pml4);

#endif
