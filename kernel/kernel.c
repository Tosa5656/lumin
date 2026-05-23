#include "drivers/vga/vga.h"
#include "drivers/serial/serial.h"
#include "drivers/rtc/rtc.h"
#include "idt.h"
#include "drivers/timer/timer.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "drivers/pci/pci.h"
#include "drivers/ata/ata.h"
#include "block/block.h"
#include "fs/vfs.h"
#include "fs/fat32.h"
#include "keyboard.h"
#include "shell.h"
#include "drivers/acpi/acpi.h"

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

void Quit(void) {
    acpi_shutdown();
}

void kmain(void)
{
    unsigned char color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    vga_init();
    vga_clear(color);
    vga_default_color = color;
    vga_write("Hello World!", color);

    serial_init();
    serial_write("Serial initialized.\n");

    keyboard_color = color;

    idt_init();
    serial_write("IDT initialized (exceptions + IRQs).\n");

    pmm_init();
    serial_printf("PMM: %d free pages\n", pmm_free_count());
    kmalloc_init();

    timer_init(1000);

    unsigned char green = vga_entry_color(VGA_GREEN, VGA_BLACK);
    vga_write("\nTimer: ", color);
    vga_write(timer_name(timer_get_type()), green);
    serial_printf("Timer: %s\n", timer_name(timer_get_type()));

    pci_scan();

    struct rtc_time tm;
    rtc_read(&tm);
    serial_printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                  tm.year, tm.month, tm.day,
                  tm.hour, tm.minute, tm.second);

    vga_printf("\nRTC: %04d-%02d-%02d %02d:%02d:%02d",
               tm.year, tm.month, tm.day,
               tm.hour, tm.minute, tm.second);

    block_init();

    int ata_count = ata_init();
    serial_printf("block: %d device(s) total\n", block_count());

    vfs_init();
    vfs_mount_devfs("/");
    serial_printf("vfs: devfs mounted at '/'\n");

    struct vfs_dentry de;
    serial_printf("vfs: ls '/':\n");
    for (int i = 0; vfs_readdir("/", i, &de) == 0; i++)
    {
        serial_printf("  %s (ino=%llu, size=%llu, type=%d)\n",
                      de.name, (unsigned long long)de.ino,
                      (unsigned long long)de.size, de.type);
    }

    serial_write("vfs: scanning partitions...\n");
    for (int bi = 0; bi < block_count(); bi++)
    {
        struct block_device *bdev = block_get(bi);
        if (!bdev) continue;

        int np = block_register_partitions(bdev);
        serial_printf("  %s: %d partition(s)\n", bdev->name, np);
    }

    int fat32_mounted = 0;
    for (int pi = 0; pi < block_partition_count() && !fat32_mounted; pi++)
    {
        struct block_device *pbdev = block_partition_get(pi);
        if (!pbdev) continue;

        if (vfs_mount_fat32("/mnt", pbdev) == 0)
        {
            serial_printf("fat32: mounted partition %s at /mnt\n", pbdev->name);
            fat32_mounted = 1;
        }
    }

    for (int bi = 0; bi < block_count() && !fat32_mounted; bi++)
    {
        struct block_device *raw = block_get(bi);
        if (!raw) continue;

        if (vfs_mount_fat32("/mnt", raw) == 0)
        {
            serial_printf("fat32: mounted raw device %s at /mnt\n", raw->name);
            fat32_mounted = 1;
        }
    }

    if (fat32_mounted)
    {
        serial_printf("vfs: ls '/mnt':\n");
        for (int i = 0; vfs_readdir("/mnt", i, &de) == 0; i++)
        {
            serial_printf("  %s (ino=%llu, size=%llu, type=%d)\n",
                          de.name, (unsigned long long)de.ino,
                          (unsigned long long)de.size, de.type);
        }

        struct vfs_file *f = vfs_open("/mnt", VFS_O_READ);
        if (f)
        {
            uint8_t buf[512];
            int r = vfs_read(f, 512, buf);
            serial_printf("fat32: read %d bytes from root dir\n", r);
            vfs_close(f);
        }
    }
    else
    {
        serial_write("fat32: no FAT32 volume found\n");
    }

    __asm__("sti");
    keyboard_init();

    serial_write("shell: starting...\n");
    shell_run();

    Quit();
}