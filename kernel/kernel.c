#include "drivers/vga/vga.h"
#include "drivers/fb/fb.h"
#include "drivers/serial/serial.h"
#include "drivers/rtc/rtc.h"
#include "gdt.h"
#include "idt.h"
#include "drivers/timer/timer.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "mm/vmm.h"
#include "proc/task.h"
#include "proc/elf.h"
#include "drivers/pci/pci.h"
#include "drivers/ata/ata.h"
#include "block/block.h"
#include "fs/vfs.h"
#include "fs/fat32.h"
#include "keyboard.h"
#include "include/initcall.h"


unsigned char keyboard_color;

static const char *timer_name(enum timer_type t)
{
    switch (t)
    {
        case TIMER_PIT:   return "PIT";
        case TIMER_HPET:  return "HPET";
        case TIMER_LAPIC: return "LAPIC";
        default:          return "NONE";
    }
}

static void run_init(void)
{
    serial_write("init: starting shell.elf...\n");
    int pid = task_create_user("/system/bin/shell.elf", 0, NULL);
    if (pid < 0)
    {
        serial_write("init: failed to create shell task\n");
        return;
    }

    serial_write("init: tasks created, switching to userspace...\n");

    __asm__("sti");

    for (;;)
    {
        __asm__ volatile("sti; hlt");
    }
}

void kmain(void)
{
    unsigned char color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    vga_init();
    vga_clear(color);
    vga_default_color = color;

    serial_init();
    serial_write("Serial initialized.\n");

    keyboard_color = color;

    gdt_init();

    idt_init();
    serial_write("IDT initialized (exceptions + IRQs).\n");

    /* Enable FPU + SSE */
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9) | (1 << 10); /* OSFXSR | OSXMMEXCPT */
    cr4 &= ~(1 << 2);            /* clear EM (FPU emulation) */
    cr4 |= (1 << 1);             /* set MP (monitor FPU) */
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    /* Initialize x87 FPU */
    __asm__ volatile("fwait; fninit");
    /* Initialize SSE MXCSR to default (masked, round-to-nearest) */
    uint32_t mxcsr = 0x1F80;
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    serial_write("FPU/SSE: enabled\n");

    pmm_init();
    int free_pages = pmm_free_count();
    serial_printf("PMM: %d free pages\n", free_pages);

    kmalloc_init();

    fb_init();
    if (fb_available)
        serial_printf("Framebuffer: %dx%d %dbpp\n", fb_width, fb_height, 32);
    else
        serial_write("Framebuffer: not available\n");

    fb_printf("lumin kernel — %d free pages\n", free_pages);

    timer_init(1000);

    unsigned char green = vga_entry_color(VGA_GREEN, VGA_BLACK);
    vga_write("\nTimer: ", color);
    vga_write(timer_name(timer_get_type()), green);
    serial_printf("Timer: %s\n", timer_name(timer_get_type()));
    fb_printf("Timer: %s\n", timer_name(timer_get_type()));

    pci_scan();

    struct rtc_time tm;
    rtc_read(&tm);
    serial_printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                  tm.year, tm.month, tm.day,
                  tm.hour, tm.minute, tm.second);
    fb_printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
              tm.year, tm.month, tm.day,
              tm.hour, tm.minute, tm.second);

    vga_printf("\nRTC: %04d-%02d-%02d %02d:%02d:%02d",
               tm.year, tm.month, tm.day,
               tm.hour, tm.minute, tm.second);

    block_init();

    int ata_count = ata_init();
    serial_printf("block: %d device(s) total\n", block_count());
    fb_printf("block: %d device(s)\n", block_count());

    vfs_init();

    serial_write("mounting sd1 as fat32...\n");
    fb_puts("mounting sd1 as fat32...\n");
    struct block_device *sd1 = block_get(1);
    if (sd1)
    {
        serial_printf("  device: %s (%llu sectors)\n", sd1->name, sd1->sector_count);
        if (vfs_mount_fat32("/", sd1) == 0)
        {
            serial_write("  mounted at /\n");
            fb_puts("  mounted at /\n");
        }
        else
        {
            serial_write("  mount FAILED (not FAT32?)\n");
            fb_puts("  mount FAILED\n");
        }
    }

    vfs_mount_devfs("/dev");
    serial_printf("vfs: devfs mounted at '/dev'\n");
    fb_puts("devfs mounted at /dev\n");
    do_initcalls();

    serial_write("vfs: scanning partitions...\n");
    fb_puts("partitions:\n");
    for (int bi = 0; bi < block_count(); bi++)
    {
        struct block_device *bdev = block_get(bi);
        if (!bdev) continue;

        int np = block_register_partitions(bdev);
        serial_printf("  %s: %d partition(s)\n", bdev->name, np);
        fb_printf("  %s: %d\n", bdev->name, np);
    }

    serial_write("devices in /dev:\n");
    fb_puts("/dev:\n");
    struct vfs_dentry de;
    for (int i = 0; vfs_readdir("/dev", i, &de) == 0; i++)
    {
        if (de.name[0] == '.' && (de.name[1] == '\0' || (de.name[1] == '.' && de.name[2] == '\0')))
            continue;
        serial_printf("  %s (size=%llu, type=%d)\n",
                      de.name, (unsigned long long)de.size, de.type);
        fb_printf("  %s\n", de.name);
    }

    task_init();

    run_init();

    serial_write("kernel: back from init, starting shell\n");
    fb_puts("starting shell...\n");

    __asm__("sti");
    keyboard_init();

    serial_write("shell: starting...\n");
}
