#include "syscall.h"
#include "task.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../drivers/serial/serial.h"
#include "../drivers/vga/vga.h"

#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_BRK   45
#define SYS_EXIT  60

uint64_t syscall_entry(struct syscall_frame *frame)
{
    uint64_t nr = frame->rax;

    if (nr != SYS_WRITE)
        serial_printf("syscall: nr=%llu from RIP=0x%p\n",
                      (unsigned long long)nr, (void*)frame->rip);

    switch (nr)
    {
        case SYS_WRITE:
        {
            int fd = (int)frame->rdi;
            const char *buf = (const char *)frame->rsi;
            uint64_t count = frame->rdx;

            if (fd == 1 || fd == 2)
            {
                for (uint64_t i = 0; i < count; i++)
                {
                    serial_putchar(buf[i]);
                    vga_putchar(buf[i], vga_default_color);
                }
            }

            return count;
        }

        case SYS_READ:
            return 0;

        case SYS_BRK:
        {
            uint64_t new_brk = frame->rdi;

            if (new_brk == 0)
                return current_brk;

            if (new_brk < USER_HEAP_START)
                return -1;

            if (new_brk < current_brk)
            {
                current_brk = new_brk;
                return current_brk;
            }

            uint64_t start_page = (current_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end_page   = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            for (uint64_t virt = start_page; virt < end_page; virt += PAGE_SIZE)
            {
                void *phys = pmm_alloc();
                if (!phys)
                {
                    serial_write("brk: pmm_alloc failed\n");
                    return -1;
                }

                if (vmm_map_page(current_pml4, virt, (uint64_t)phys, PAGE_USER | PAGE_WRITE) != 0)
                {
                    serial_write("brk: vmm_map_page failed\n");
                    pmm_free(phys);
                    return -1;
                }
            }

            current_brk = new_brk;
            return current_brk;
        }

        case SYS_EXIT:
        {
            serial_printf("syscall: exit(%llu)\n", (unsigned long long)frame->rdi);
            __asm__ volatile(
                "cli\n"
                "mov %0, %%cr3\n"
                "mov %1, %%rsp\n"
                "pop %%rbp\n"
                "ret\n"
                : : "r"(saved_kernel_cr3), "r"(saved_kernel_rsp)
                : "memory"
            );
            __builtin_unreachable();
            return 0;
        }

        default:
            serial_printf("syscall: unknown nr %llu\n", (unsigned long long)nr);
            return -1;
    }
}
