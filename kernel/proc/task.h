#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 32
#define KSTACK_SIZE 0x4000
#define KSTACK_PAGES (KSTACK_SIZE / 0x1000)
#define USER_HEAP_START (USER_BASE_VADDR + 0x100000000ULL)
#define MAX_FDS 16

struct vfs_file;

enum task_state {
    TASK_FREE    = 0,
    TASK_READY   = 1,
    TASK_RUNNING = 2,
    TASK_BLOCKED = 3,
    TASK_ZOMBIE  = 4,
};

struct cpu_iretq_frame {
    uint64_t rip, cs, rflags, rsp, ss;
};

struct pushaq_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    struct cpu_iretq_frame iretq;
};

#include "../include/signal.h"

struct task {
    int pid;
    enum task_state state;
    uint64_t kernel_rsp;
    uint64_t kernel_cr3;
    uint64_t *pml4;
    uint64_t brk;
    void    *kstack_page;
    void    *ustack_page;
    int exit_code;
    int parent_pid;
    struct vfs_file *fds[MAX_FDS];
    char cwd[512];
    sigset_t sig_pending;
    sigset_t sig_blocked;
    struct sigaction sig_actions[32];
};

int  task_init(void);
int  task_create_user(const char *path, int argc, char **argv);
void task_exit(int code);
int  task_fork(struct pushaq_frame *frame);
int  task_getpid(void);
int  task_waitpid_nb(int pid);
int  task_waitpid(int pid);
void task_yield(void);
uint64_t schedule(uint64_t current_rsp);
int  task_kill(int pid, int sig);
int  task_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
void task_check_signals(void);

extern struct task *current_task;

#endif