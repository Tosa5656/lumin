#include "syscall.h"
#include "task.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../drivers/serial/serial.h"
#include "../drivers/vga/vga.h"
#include "../drivers/fb/fb.h"
#include "../fs/vfs.h"
#include "../fs/pipefs.h"
#include "../ports.h"
#include "keyboard.h"

#define SYS_READ     0
#define SYS_WRITE    1
#define SYS_OPEN     2
#define SYS_CLOSE    3
#define SYS_PIPE     4
#define SYS_READDIR  5
#define SYS_STAT     6
#define SYS_CLEAR    7
#define SYS_REBOOT   8
#define SYS_DUP      9
#define SYS_DUP2     10
#define SYS_KILL     11
#define SYS_SIGACTION 12
#define SYS_SIGRETURN 13
#define SYS_YIELD    24
#define SYS_GETPID   39
#define SYS_BRK      45
#define SYS_FORK     57
#define SYS_SPAWN    59
#define SYS_EXIT     60
#define SYS_WAITPID  61
#define SYS_GETCWD   79
#define SYS_CHDIR    80

/* TEMP FUNCTION */
static void normalize_path(const char *cwd, const char *input, char *output, uint64_t max_len)
{
    char temp[256];
    uint64_t t_idx = 0;

    if (input[0] == '/')
    {
        temp[t_idx++] = '/';
        input++;
    }
    else
    {
        uint64_t i = 0;
        while (cwd[i] != '\0' && t_idx < sizeof(temp) - 1)
        {
            temp[t_idx++] = cwd[i++];
        }
        if (temp[t_idx - 1] != '/')
        {
            temp[t_idx++] = '/';
        }
    }
    temp[t_idx] = '\0';

    uint64_t in_idx = 0;
    while (input[in_idx] != '\0')
    {
        if (input[in_idx] == '/')
        {
            in_idx++;
            continue;
        }

        char segment[64];
        uint64_t s_idx = 0;
        while (input[in_idx] != '/' && input[in_idx] != '\0' && s_idx < sizeof(segment) - 1)
        {
            segment[s_idx++] = input[in_idx++];
        }
        segment[s_idx] = '\0';

        if (segment[0] == '\0' || (segment[0] == '.' && segment[1] == '\0'))
        {
            continue;
        } 
        else if (segment[0] == '.' && segment[1] == '.' && segment[2] == '\0')
        {
            if (t_idx > 1) {
                t_idx--;
                while (t_idx > 0 && temp[t_idx] != '/') {
                    t_idx--;
                }
                if (t_idx == 0) t_idx = 1;
                temp[t_idx] = '\0';
            }
        } 
        else
        {
            if (t_idx > 1 && temp[t_idx - 1] != '/') {
                if (t_idx < sizeof(temp) - 1) temp[t_idx++] = '/';
            }
            uint64_t i = 0;
            while (segment[i] != '\0' && t_idx < sizeof(temp) - 1) {
                temp[t_idx++] = segment[i++];
            }
            temp[t_idx] = '\0';
        }
    }

    uint64_t i = 0;
    while (temp[i] != '\0' && i < max_len - 1)
    {
        output[i] = temp[i];
        i++;
    }
    output[i] = '\0';
}


uint64_t syscall_entry(struct pushaq_frame *frame)
{
    uint64_t nr = frame->rax;
    uint64_t result = 0;

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
                    if (fb_available)
                        fb_putchar(buf[i]);
                }
            }

            result = count;
            break;
        }

        case SYS_READ:
        {
            int fd = (int)frame->rdi;
            char *buf = (char *)frame->rsi;
            uint64_t count = frame->rdx;

            if (fd == 0)
            {
                uint64_t read = 0;
                while (read < count)
                {
                    while (!keyboard_avail())
                        __asm__ volatile("sti; hlt; cli");

                    int c = keyboard_dequeue();
                    if (c < 0) continue;

                    buf[read++] = (char)c;
                }
                result = read;
                break;
            }

            if (!current_task || fd < 0 || fd >= MAX_FDS || !current_task->fds[fd])
                result = -1;
            else
                result = vfs_read(current_task->fds[fd], count, buf);
            break;
        }

        case SYS_OPEN:
        {
            const char *path = (const char *)frame->rdi;
            int flags = (int)frame->rsi;

            if (!current_task || current_task->pid == 0)
            {
                result = -1;
                break;
            }

            struct vfs_file *f = vfs_open(path, flags);
            if (!f)
            {
                result = -1;
                break;
            }

            int fd = -1;
            for (int i = 0; i < MAX_FDS; i++)
            {
                if (!current_task->fds[i])
                {
                    fd = i;
                    break;
                }
            }
            if (fd < 0)
            {
                vfs_close(f);
                result = -1;
                break;
            }

            current_task->fds[fd] = f;
            result = fd;
            break;
        }

        case SYS_CLOSE:
        {
            int fd = (int)frame->rdi;
            if (!current_task || fd < 0 || fd >= MAX_FDS || !current_task->fds[fd])
                result = -1;
            else
            {
                vfs_close(current_task->fds[fd]);
                current_task->fds[fd] = NULL;
                result = 0;
            }
            break;
        }

        case SYS_READDIR:
        {
            const char *path = (const char *)frame->rdi;
            uint32_t index = (uint32_t)frame->rsi;
            struct vfs_dentry *entry = (struct vfs_dentry *)frame->rdx;

            result = vfs_readdir(path, index, entry);
            break;
        }

        case SYS_STAT:
        {
            const char *path = (const char *)frame->rdi;
            struct vfs_dentry *entry = (struct vfs_dentry *)frame->rsi;

            result = vfs_stat(path, entry);
            break;
        }

        case SYS_CLEAR:
            vga_clear(vga_default_color);
            if (fb_available)
                fb_clear(fb_rgb(0, 0, 0));
            result = 0;
            break;

        case SYS_REBOOT:
            serial_write("REBOOT\n");
            outb(0x64, 0xFE);
            result = 0;
            break;

        case SYS_BRK:
        {
            uint64_t new_brk = frame->rdi;

            if (!current_task || current_task->pid == 0)
            {
                result = -1;
                break;
            }

            if (new_brk == 0)
            {
                result = current_task->brk;
                break;
            }

            if (new_brk < USER_HEAP_START)
            {
                result = -1;
                break;
            }

            if (new_brk < current_task->brk)
            {
                current_task->brk = new_brk;
                result = current_task->brk;
                break;
            }

            uint64_t start_page = (current_task->brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end_page   = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            for (uint64_t virt = start_page; virt < end_page; virt += PAGE_SIZE)
            {
                void *phys = pmm_alloc();
                if (!phys)
                {
                    result = -1;
                    break;
                }

                if (vmm_map_page(current_task->pml4, virt, (uint64_t)phys, PAGE_USER | PAGE_WRITE) != 0)
                {
                    pmm_free(phys);
                    result = -1;
                    break;
                }
            }
            if (result == 0)
            {
                current_task->brk = new_brk;
                result = current_task->brk;
            }
            break;
        }

        case SYS_GETPID:
            result = task_getpid();
            break;

        case SYS_YIELD:
        {
            if (current_task && current_task->pid != 0)
                current_task->state = TASK_READY;
            __asm__ volatile("cli");
            return schedule((uint64_t)frame);
        }

        case SYS_FORK:
            result = task_fork(frame);
            break;

        case SYS_SPAWN:
        {
            const char *path = (const char *)frame->rdi;
            int argc = (int)frame->rsi;
            char **argv = (char **)frame->rdx;
            result = task_create_user(path, argc, argv);
            break;
        }

        case SYS_WAITPID:
            result = task_waitpid_nb((int)frame->rdi);
            break;

        case SYS_EXIT:
            task_exit((int)frame->rdi);
            result = 0;
            break;
        
        case SYS_GETCWD:
        {
            char *user_buf = (char *)frame->rdi;
            uint64_t size = frame->rsi;

            if (!current_task || current_task->pid == 0 || !user_buf || size == 0)
            {
                result = -1;
                break;
            }

            uint64_t path_len = 0;
            while (current_task->cwd[path_len] != '\0')
            {
                path_len++;
            }

            if (path_len >= size)
            {
                result = -1;
                break;
            }
            
            for (uint64_t i = 0; i <= path_len; i++)
            {
                user_buf[i] = current_task->cwd[i];
            }

            result = 0;
            break;
        }
        case SYS_CHDIR:
        {
            const char *user_path = (const char *)frame->rdi;

            if (!current_task || current_task->pid == 0 || !user_path)
            {
                result = -1;
                break;
            }

            char target_path[256];
            normalize_path(current_task->cwd, user_path, target_path, sizeof(target_path));

            struct vfs_dentry entry;
            int stat_res = vfs_stat(target_path, &entry);
            
            if (stat_res != 0)
            {
                result = -1;
                break;
            }
            
            uint64_t i = 0;
            while (target_path[i] != '\0' && i < sizeof(current_task->cwd) - 1)
            {
                current_task->cwd[i] = target_path[i];
                i++;
            }
            current_task->cwd[i] = '\0';

            result = 0;
            break;
        }
        case SYS_PIPE:
        {
            int *pipefd = (int *)frame->rdi;
            int fds[2];
            if (vfs_pipe_create(&fds[0], &fds[1]) != 0)
            {
                result = -1;
                break;
            }
            pipefd[0] = fds[0];
            pipefd[1] = fds[1];
            result = 0;
            break;
        }

        case SYS_DUP:
        {
            int oldfd = (int)frame->rdi;
            if (!current_task || oldfd < 0 || oldfd >= MAX_FDS || !current_task->fds[oldfd])
            {
                result = -1;
                break;
            }
            int newfd = -1;
            for (int i = 0; i < MAX_FDS; i++)
            {
                if (!current_task->fds[i])
                {
                    newfd = i;
                    break;
                }
            }
            if (newfd < 0)
            {
                result = -1;
                break;
            }
            current_task->fds[newfd] = vfs_dup(current_task->fds[oldfd]);
            result = newfd;
            break;
        }

        case SYS_DUP2:
        {
            int oldfd = (int)frame->rdi;
            int newfd = (int)frame->rsi;
            if (!current_task || oldfd < 0 || oldfd >= MAX_FDS || newfd < 0 || newfd >= MAX_FDS)
            {
                result = -1;
                break;
            }
            if (!current_task->fds[oldfd])
            {
                result = -1;
                break;
            }
            if (current_task->fds[newfd])
            {
                vfs_close(current_task->fds[newfd]);
                current_task->fds[newfd] = NULL;
            }
            current_task->fds[newfd] = vfs_dup(current_task->fds[oldfd]);
            result = newfd;
            break;
        }

        case SYS_KILL:
        {
            int pid = (int)frame->rdi;
            int sig = (int)frame->rsi;
            result = task_kill(pid, sig);
            break;
        }

        case SYS_SIGACTION:
        {
            int sig = (int)frame->rdi;
            const struct sigaction *act = (const struct sigaction *)frame->rsi;
            struct sigaction *oldact = (struct sigaction *)frame->rdx;
            result = task_sigaction(sig, act, oldact);
            break;
        }

        case SYS_SIGRETURN:
        {
            result = 0;
            break;
        }

        default:
            serial_printf("syscall: unknown nr %llu from pid=%d\n",
                          (unsigned long long)nr, current_task ? current_task->pid : -1);
            result = -1;
            break;
    }

    frame->rax = result;
    task_check_signals();
    return (uint64_t)frame;
}
