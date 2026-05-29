#include "serial.h"
#include <stddef.h>
#include "ports.h"
#include "kprintf.h"
#include "fs/vfs.h"
#include "include/initcall.h"

static int serial_chr_read(struct vfs_inode *inode, uint64_t offset, uint64_t count, void *buf)
{
    (void)inode; (void)offset;
    unsigned char *cbuf = (unsigned char *)buf;
    int i;
    for (i = 0; i < (int)count; i++)
    {
        int c = serial_readchar();
        if (c < 0) break;
        cbuf[i] = (unsigned char)c;
    }
    return i ? i : (count ? -1 : 0);
}

static int serial_chr_write(struct vfs_inode *inode, uint64_t offset, uint64_t count, const void *buf)
{
    (void)inode; (void)offset;
    const char *cbuf = (const char *)buf;
    for (uint64_t i = 0; i < count; i++)
        serial_putchar(cbuf[i]);
    return (int)count;
}

static struct vfs_inode_ops serial_chr_ops = {
    .read  = serial_chr_read,
    .write = serial_chr_write,
};

static int serial_chr_init(void)
{
    devfs_register_chrdev("ttyS0", &serial_chr_ops, NULL);
    return 0;
}
pure_initcall(serial_chr_init);

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

int serial_readchar(void)
{
    if (inb(COM1 + 5) & 1)
        return inb(COM1);
    return -1;
}

void serial_putchar(char c)
{
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, c);
}

void serial_write(const char *str)
{
    while (*str)
        serial_putchar(*str++);
}

static void serial_putch(char c, void *ctx)
{
    (void)ctx;
    serial_putchar(c);
}

void serial_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvformat(serial_putch, NULL, fmt, ap);
    va_end(ap);
}