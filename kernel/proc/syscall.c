#include "syscall.h"
#include "task.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../drivers/serial/serial.h"
#include "../drivers/vga/vga.h"
#include "keyboard.h"

#define SYS_READ     0
#define SYS_WRITE    1
#define SYS_YIELD    24
#define SYS_GETPID   39
#define SYS_BRK      45
#define SYS_FORK     57
#define SYS_SPAWN    59
#define SYS_EXIT     60
#define SYS_WAITPID  61

uint64_t syscall_entry(struct pushaq_frame *frame)
{
    uint64_t nr = frame->rax;

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
        {
            int fd = (int)frame->rdi;
            char *buf = (char *)frame->rsi;
            uint64_t count = frame->rdx;

            if (fd != 0)
                return -1;

            uint64_t read = 0;
            while (read < count)
            {
                while (!keyboard_avail())
                    __asm__ volatile("sti; hlt; cli");

                int c = keyboard_dequeue();
                if (c < 0) continue;

                buf[read++] = (char)c;
            }
            return read;
        }

        case SYS_BRK:
        {
            uint64_t new_brk = frame->rdi;

            if (!current_task || current_task->pid == 0)
                return -1;

            if (new_brk == 0)
                return current_task->brk;

            if (new_brk < USER_HEAP_START)
                return -1;

            if (new_brk < current_task->brk)
            {
                current_task->brk = new_brk;
                return current_task->brk;
            }

            uint64_t start_page = (current_task->brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end_page   = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            for (uint64_t virt = start_page; virt < end_page; virt += PAGE_SIZE)
            {
                void *phys = pmm_alloc();
                if (!phys)
                    return -1;

                if (vmm_map_page(current_task->pml4, virt, (uint64_t)phys, PAGE_USER | PAGE_WRITE) != 0)
                {
                    pmm_free(phys);
                    return -1;
                }
            }

            current_task->brk = new_brk;
            return current_task->brk;
        }

        case SYS_GETPID:
            return task_getpid();

        case SYS_YIELD:
            return 0;

        case SYS_FORK:
            return task_fork(frame);

        case SYS_SPAWN:
        {
            const char *path = (const char *)frame->rdi;
            int argc = (int)frame->rsi;
            char **argv = (char **)frame->rdx;
            return task_create_user(path, argc, argv);
        }

        case SYS_WAITPID:
            return task_waitpid_nb((int)frame->rdi);

        case SYS_EXIT:
            task_exit((int)frame->rdi);
            return 0;

        default:
            serial_printf("syscall: unknown nr %llu from pid=%d\n",
                          (unsigned long long)nr, current_task ? current_task->pid : -1);
            return -1;
    }
}
