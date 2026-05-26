#include "task.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../gdt.h"
#include "../drivers/serial/serial.h"

uint64_t saved_kernel_rsp;
uint64_t saved_kernel_cr3;

struct iretq_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

int task_init(void)
{
    serial_write("task: initialized\n");
    return 0;
}

void exec_user(uint64_t entry, uint64_t *pml4)
{
    __asm__ volatile("mov %%rbp, %0" : "=r"(saved_kernel_rsp) : : "memory");
    __asm__ volatile("mov %%cr3, %0" : "=r"(saved_kernel_cr3) : : "memory");

    void *kstack = pmm_alloc();
    if (!kstack)
    {
        serial_write("exec: pmm_alloc kstack failed\n");
        return;
    }

    uint64_t kstack_top = (uint64_t)kstack + 0x1000;

    void *ustack = pmm_alloc();
    if (!ustack)
    {
        serial_write("exec: pmm_alloc ustack failed\n");
        return;
    }

    uint64_t us_vaddr = USER_BASE_VADDR + 0x1000000000ULL;
    if (vmm_map_page(pml4, us_vaddr - 0x1000, (uint64_t)ustack, PAGE_USER | PAGE_WRITE) != 0)
    {
        serial_write("exec: failed to map user stack\n");
        return;
    }

    uint64_t *area = (uint64_t *)(kstack_top - 15 * 8 - 5 * 8);
    for (int i = 0; i < 15; i++)
        area[i] = 0;
    area[15] = entry;
    area[16] = GDT_UCODE | 3;
    area[17] = 0x202;
    area[18] = us_vaddr;
    area[19] = GDT_UDATA | 3;

    gdt_set_kstack((uint64_t)kstack + 0x1000);

    serial_printf("exec: entry=0x%p kstack=0x%p ustack=0x%p\n",
                  (void*)entry, (void*)kstack, (void*)ustack);

    if (pml4)
        vmm_load_pml4(pml4);

    __asm__ volatile(
        "mov %0, %%rsp\n"
        "pop %%r15\n  pop %%r14\n  pop %%r13\n  pop %%r12\n"
        "pop %%r11\n  pop %%r10\n  pop %%r9\n   pop %%r8\n"
        "pop %%rbp\n  pop %%rdi\n  pop %%rsi\n  pop %%rdx\n"
        "pop %%rcx\n  pop %%rbx\n  pop %%rax\n"
        "iretq\n"
        : : "r"((uint64_t)area) : "memory"
    );
}
