#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

struct syscall_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, user_rsp, ss;
};

uint64_t syscall_entry(struct syscall_frame *frame);

#endif
