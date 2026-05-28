#include "task.h"
#include "elf.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../gdt.h"
#include "../drivers/serial/serial.h"
#include "../drivers/vga/vga.h"
#include "../fs/vfs.h"

static struct task tasks[MAX_TASKS];
static int next_pid = 1;
static int task_count = 0;
static int current_idx = 0;
static unsigned int pid_bitmap[(MAX_TASKS + 31) / 32];

struct task *current_task = NULL;

static int alloc_pid(void)
{
    for (int attempt = 0; attempt < MAX_TASKS * 2; attempt++)
    {
        if (next_pid >= 1000000)
            next_pid = 1;
        int pid = next_pid++;
        int idx = pid / 32;
        int bit = pid % 32;
        if (idx >= (int)(sizeof(pid_bitmap)/sizeof(pid_bitmap[0])))
            continue;
        if (!(pid_bitmap[idx] & (1U << bit)))
        {
            pid_bitmap[idx] |= (1U << bit);
            return pid;
        }
    }
    return -1;
}

static void free_pid(int pid)
{
    int idx = pid / 32;
    int bit = pid % 32;
    if (idx < (int)(sizeof(pid_bitmap)/sizeof(pid_bitmap[0])))
        pid_bitmap[idx] &= ~(1U << bit);
}

static void free_task_resources(struct task *t)
{
    for (int i = 0; i < MAX_FDS; i++)
    {
        if (t->fds[i])
        {
            vfs_close(t->fds[i]);
            t->fds[i] = NULL;
        }
    }

    if (t->ustack_page)
    {
        pmm_free(t->ustack_page);
        t->ustack_page = NULL;
    }
    if (t->pml4)
    {
        uint64_t *pml4 = t->pml4;
        for (int pml4_idx = 1; pml4_idx < 256; pml4_idx++)
        {
            uint64_t pml4e = pml4[pml4_idx];
            if (!(pml4e & PAGE_PRESENT))
                continue;
            uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4e & ~0xFFFULL);
            for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++)
            {
                uint64_t pdpte = pdpt[pdpt_idx];
                if (!(pdpte & PAGE_PRESENT))
                    continue;
                if (pdpte & PAGE_HUGE)
                {
                    pmm_free((void *)(uintptr_t)(pdpte & ~0xFFFULL));
                    continue;
                }
                uint64_t *pd = (uint64_t *)(uintptr_t)(pdpte & ~0xFFFULL);
                for (int pd_idx = 0; pd_idx < 512; pd_idx++)
                {
                    uint64_t pde = pd[pd_idx];
                    if (!(pde & PAGE_PRESENT))
                        continue;
                    if (pde & PAGE_HUGE)
                    {
                        pmm_free((void *)(uintptr_t)(pde & ~0xFFFULL));
                        continue;
                    }
                    uint64_t *pt = (uint64_t *)(uintptr_t)(pde & ~0xFFFULL);
                    for (int pt_idx = 0; pt_idx < 512; pt_idx++)
                    {
                        uint64_t pte = pt[pt_idx];
                        if (!(pte & PAGE_PRESENT))
                            continue;
                        pmm_free((void *)(uintptr_t)(pte & ~0xFFFULL));
                    }
                    pmm_free(pt);
                }
                pmm_free(pd);
            }
            pmm_free(pdpt);
        }
        pmm_free(pml4);
        t->pml4 = NULL;
    }
    if (t->kstack_page)
    {
        pmm_free_pages(t->kstack_page, KSTACK_PAGES);
        t->kstack_page = NULL;
    }
}

static struct task *alloc_task(void)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].state == TASK_FREE)
        {
            if (tasks[i].kstack_page || tasks[i].ustack_page || tasks[i].pml4)
                free_task_resources(&tasks[i]);
            return &tasks[i];
        }
    }
    return NULL;
}

static int find_task_idx(struct task *t)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (&tasks[i] == t)
            return i;
    }
    return -1;
}

static struct task *find_task_by_pid(int pid)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].pid == pid && tasks[i].state != TASK_FREE)
            return &tasks[i];
    }
    return NULL;
}

int task_init(void)
{
    for (int i = 0; i < MAX_TASKS; i++)
        tasks[i].state = TASK_FREE;

    for (int i = 0; i < (int)(sizeof(pid_bitmap)/sizeof(pid_bitmap[0])); i++)
        pid_bitmap[i] = 0;

    tasks[0].pid          = 0;
    tasks[0].state        = TASK_RUNNING;
    tasks[0].pml4         = NULL;
    tasks[0].kstack_page  = NULL;
    tasks[0].ustack_page  = NULL;
    tasks[0].brk          = 0;
    tasks[0].parent_pid   = -1;

    __asm__ volatile("mov %%rsp, %0"  : "=r"(tasks[0].kernel_rsp));
    __asm__ volatile("mov %%cr3, %0"  : "=r"(tasks[0].kernel_cr3));

    current_task = &tasks[0];
    current_idx  = 0;
    task_count   = 1;
    next_pid     = 1;
    pid_bitmap[0] = 1;

    serial_write("scheduler: initialized (task 0 = kernel)\n");
    return 0;
}

static uint64_t str_len(const char *s)
{
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void str_cpy(char *dst, const char *src)
{
    while (*src)
        *dst++ = *src++;
    *dst = '\0';
}

static void setup_user_stack(char *phys_stack, uint64_t us_vaddr, int argc, char **argv, uint64_t *out_rsp)
{
    uint64_t off = 0x1000;

    uint64_t arg_string_offsets[64];
    if (argc > 64) argc = 64;

    for (int i = argc - 1; i >= 0; i--)
    {
        const char *s = argv && argv[i] ? argv[i] : "";
        uint64_t len = str_len(s) + 1;
        off -= len;
        str_cpy(phys_stack + off, s);
        arg_string_offsets[i] = us_vaddr - 0x1000 + off;
    }

    off -= 8;
    *(uint64_t *)(phys_stack + off) = 0;

    for (int i = argc - 1; i >= 0; i--)
    {
        off -= 8;
        *(uint64_t *)(phys_stack + off) = arg_string_offsets[i];
    }
    uint64_t argv_ptr = us_vaddr - 0x1000 + off;

    off -= 8;
    *(uint64_t *)(phys_stack + off) = (uint64_t)(uintptr_t)argc;

    *out_rsp = us_vaddr - 0x1000 + off;
}

int task_create_user(const char *path, int argc, char **argv)
{
    if (!path)
        return -1;

    struct vfs_file *f = vfs_open(path, VFS_O_READ);
    if (!f)
    {
        serial_printf("task_create: cannot open '%s'\n", path);
        return -1;
    }

    uint64_t *pml4 = vmm_create_pml4();
    if (!pml4)
    {
        serial_write("task_create: vmm_create_pml4 failed\n");
        vfs_close(f);
        return -1;
    }

    uint64_t entry = elf_load(f, pml4);
    vfs_close(f);

    if (!entry)
    {
        serial_write("task_create: elf_load failed\n");
        return -1;
    }

    struct task *t = alloc_task();
    if (!t)
    {
        serial_write("task_create: no free task slot\n");
        return -1;
    }

    void *kstack = pmm_alloc_pages(KSTACK_PAGES);
    void *ustack = pmm_alloc();
    if (!kstack || !ustack)
    {
        serial_write("task_create: pmm_alloc failed\n");
        if (kstack) pmm_free_pages(kstack, KSTACK_PAGES);
        if (ustack) pmm_free(ustack);
        return -1;
    }

    uint64_t kstack_top = (uint64_t)kstack + KSTACK_SIZE;
    uint64_t us_vaddr   = USER_BASE_VADDR + 0x1000000000ULL;

    uint64_t user_rsp;
    setup_user_stack((char *)ustack, us_vaddr, argc, argv, &user_rsp);

    if (vmm_map_page(pml4, us_vaddr - 0x1000, (uint64_t)ustack, PAGE_USER | PAGE_WRITE) != 0)
    {
        serial_write("task_create: map ustack failed\n");
        pmm_free_pages(kstack, KSTACK_PAGES);
        pmm_free(ustack);
        return -1;
    }

    uint64_t *sp = (uint64_t *)(kstack_top - 5 * 8);
    sp[0] = entry;
    sp[1] = GDT_UCODE | 3;
    sp[2] = 0x202;
    sp[3] = user_rsp;
    sp[4] = GDT_UDATA | 3;

    sp -= 15;
    for (int i = 0; i < 15; i++)
        sp[i] = 0;

    t->pid          = alloc_pid();
    t->state        = TASK_READY;
    t->kernel_rsp   = (uint64_t)sp;
    t->kernel_cr3   = tasks[0].kernel_cr3;
    t->pml4         = pml4;
    t->brk          = USER_HEAP_START;
    t->kstack_page  = kstack;
    t->ustack_page  = ustack;
    t->exit_code    = 0;
    t->parent_pid   = current_task ? current_task->pid : 0;
    for (int i = 0; i < MAX_FDS; i++)
        t->fds[i] = NULL;

    task_count++;

    serial_printf("task_create: pid=%d entry=0x%p\n", t->pid, (void*)entry);
    return t->pid;
}

void task_exit(int code)
{
    if (!current_task || current_task->pid == 0)
    {
        serial_printf("exit(%d) in kernel task, halting\n", code);
        __asm__("cli; hlt");
        while (1) {}
    }

    serial_printf("task_exit: pid=%d code=%d\n", current_task->pid, code);
    free_pid(current_task->pid);
    current_task->state = TASK_FREE;
    current_task->exit_code = code;

    uint64_t kernel_cr3 = tasks[0].kernel_cr3;
    uint64_t kernel_rsp = tasks[0].kernel_rsp;

    tasks[0].state = TASK_RUNNING;
    current_task = &tasks[0];
    current_idx  = 0;

    __asm__ volatile(
        "cli\n"
        "mov %0, %%cr3\n"
        "mov %1, %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%r11\n"
        "pop %%r10\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%rbp\n"
        "pop %%rdi\n"
        "pop %%rsi\n"
        "pop %%rdx\n"
        "pop %%rcx\n"
        "pop %%rbx\n"
        "pop %%rax\n"
        "iretq\n"
        : : "r"(kernel_cr3), "r"(kernel_rsp)
        : "memory"
    );

    __builtin_unreachable();
}

int task_fork(struct pushaq_frame *frame)
{
    if (!current_task)
        return -1;

    struct task *t = alloc_task();
    if (!t)
        return -1;

    uint64_t *pml4 = vmm_create_pml4();
    if (!pml4)
        return -1;

    for (int i = 256; i < 512; i++)
    {
        uint64_t *pt;
        __asm__("mov %%cr3, %%rax; mov %%rax, %0" : "=r"(pt));
        (void)pt;
    }

    void *kstack = pmm_alloc_pages(KSTACK_PAGES);
    void *ustack = pmm_alloc();
    if (!kstack || !ustack)
    {
        if (kstack) pmm_free_pages(kstack, KSTACK_PAGES);
        if (ustack) pmm_free(ustack);
        return -1;
    }

    uint64_t kstack_top = (uint64_t)kstack + KSTACK_SIZE;
    uint64_t us_vaddr   = USER_BASE_VADDR + 0x1000000000ULL;

    uint64_t *parent_kstack = (uint64_t *)current_task->kernel_rsp;
    uint64_t frame_words = 15;
    if ((parent_kstack[16] & 3) == 3)
        frame_words = 20;

    uint64_t *sp = (uint64_t *)(kstack_top - frame_words * 8);
    for (uint64_t i = 0; i < frame_words; i++)
        sp[i] = parent_kstack[i];

    sp[14] = 0;

    uint64_t parent_cr3_val;
    __asm__("mov %%cr3, %0" : "=r"(parent_cr3_val));
    uint64_t *parent_pml4_phys = (uint64_t *)(uintptr_t)parent_cr3_val;

    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++)
    {
        uint64_t pml4e = parent_pml4_phys[pml4_idx];
        if (!(pml4e & PAGE_PRESENT))
            continue;

        uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4e & ~0xFFFULL);
        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++)
        {
            uint64_t pdpte = pdpt[pdpt_idx];
            if (!(pdpte & PAGE_PRESENT))
                continue;

            if (pdpte & PAGE_HUGE)
            {
                void *phys = pmm_alloc();
                if (!phys) return -1;

                uint64_t vaddr = ((uint64_t)pml4_idx << 39) | ((uint64_t)pdpt_idx << 30);
                vmm_map_page(pml4, vaddr, (uint64_t)phys, pdpte & 0xFFF);

                void *src = (void *)(uintptr_t)(pdpte & ~0xFFFULL);
                for (uint64_t o = 0; o < 0x40000000; o += 8)
                    ((uint64_t *)phys)[o / 8] = ((uint64_t *)src)[o / 8];
                continue;
            }

            uint64_t *pd = (uint64_t *)(uintptr_t)(pdpte & ~0xFFFULL);
            for (int pd_idx = 0; pd_idx < 512; pd_idx++)
            {
                uint64_t pde = pd[pd_idx];
                if (!(pde & PAGE_PRESENT))
                    continue;

                if (pde & PAGE_HUGE)
                {
                    void *phys = pmm_alloc();
                    if (!phys) return -1;

                    uint64_t vaddr = ((uint64_t)pml4_idx << 39) | ((uint64_t)pdpt_idx << 30) | ((uint64_t)pd_idx << 21);
                    vmm_map_page(pml4, vaddr, (uint64_t)phys, pde & 0xFFF);

                    void *src = (void *)(uintptr_t)(pde & ~0xFFFULL);
                    for (uint64_t o = 0; o < 0x200000; o += 8)
                        ((uint64_t *)phys)[o / 8] = ((uint64_t *)src)[o / 8];
                    continue;
                }

                uint64_t *pt = (uint64_t *)(uintptr_t)(pde & ~0xFFFULL);
                for (int pt_idx = 0; pt_idx < 512; pt_idx++)
                {
                    uint64_t pte = pt[pt_idx];
                    if (!(pte & PAGE_PRESENT))
                        continue;

                    void *phys = pmm_alloc();
                    if (!phys) return -1;

                    uint64_t vaddr = ((uint64_t)pml4_idx << 39) | ((uint64_t)pdpt_idx << 30) |
                                     ((uint64_t)pd_idx << 21) | ((uint64_t)pt_idx << 12);
                    vmm_map_page(pml4, vaddr, (uint64_t)phys, pte & 0xFFF);

                    void *src = (void *)(uintptr_t)(pte & ~0xFFFULL);
                    for (uint64_t o = 0; o < 0x1000; o += 8)
                        ((uint64_t *)phys)[o / 8] = ((uint64_t *)src)[o / 8];
                }
            }
        }
    }

    vmm_map_page(pml4, us_vaddr - 0x1000, (uint64_t)ustack, PAGE_USER | PAGE_WRITE);

    int child_pid = alloc_pid();
    t->pid         = child_pid;
    t->state       = TASK_READY;
    t->kernel_rsp  = (uint64_t)sp;
    t->kernel_cr3  = current_task->kernel_cr3;
    t->pml4        = pml4;
    t->brk         = current_task->brk;
    t->kstack_page = kstack;
    t->ustack_page = ustack;
    t->exit_code   = 0;
    t->parent_pid  = current_task->pid;
    for (int i = 0; i < MAX_FDS; i++)
        t->fds[i] = NULL;

    task_count++;
    serial_printf("fork: child pid=%d\n", child_pid);
    return child_pid;
}

int task_waitpid_nb(int pid)
{
    if (!current_task) return -1;

    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].parent_pid == current_task->pid &&
            (pid == -1 || tasks[i].pid == pid))
        {
            if (tasks[i].state == TASK_FREE)
                return tasks[i].exit_code;
            return -2;
        }
    }
    return -1;
}

int task_getpid(void)
{
    if (!current_task) return -1;
    return current_task->pid;
}

void task_yield(void)
{
    __asm__ volatile("int $0x30" : : "a"(45), "D"(0), "S"(0), "d"(0) : "memory");
}

uint64_t schedule(uint64_t current_rsp)
{
    if (task_count <= 1)
    {
        if (current_task)
            current_task->kernel_rsp = current_rsp;
        return current_rsp;
    }

    if (current_task && current_task->state == TASK_RUNNING)
    {
        current_task->kernel_rsp = current_rsp;
        current_task->state = TASK_READY;
    }

    for (int i = 1; i < MAX_TASKS; i++)
    {
        int idx = (current_idx + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY)
        {
            current_idx  = idx;
            current_task = &tasks[idx];
            current_task->state = TASK_RUNNING;

            if (current_task->pml4)
                vmm_load_pml4(current_task->pml4);
            else
            {
                uint64_t kcr3;
                __asm__("mov %%cr3, %0" : "=r"(kcr3));
                if (kcr3 != tasks[0].kernel_cr3)
                    __asm__ volatile("mov %0, %%cr3" : : "r"(tasks[0].kernel_cr3) : "memory");
            }

            if (current_task->kstack_page)
                gdt_set_kstack((uint64_t)current_task->kstack_page + KSTACK_SIZE);

            return current_task->kernel_rsp;
        }
    }

    current_task = &tasks[0];
    current_idx  = 0;
    tasks[0].state = TASK_RUNNING;

    uint64_t kcr3;
    __asm__("mov %%cr3, %0" : "=r"(kcr3));
    if (kcr3 != tasks[0].kernel_cr3)
        __asm__ volatile("mov %0, %%cr3" : : "r"(tasks[0].kernel_cr3) : "memory");

    return tasks[0].kernel_rsp;
}
