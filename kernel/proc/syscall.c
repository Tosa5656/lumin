#include "syscall.h"
#include "task.h"
#include "../drivers/serial/serial.h"
#include "../drivers/vga/vga.h"

#define SYS_READ  0
#define SYS_WRITE 1
#define SYS_EXIT  60

uint64_t syscall_entry(struct syscall_frame *frame)
{
    uint64_t nr = frame->rax;

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
