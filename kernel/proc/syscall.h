#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

struct pushaq_frame;

uint64_t syscall_entry(struct pushaq_frame *frame);

#endif
