#include "serial.h"
#include <stddef.h>
#include "ports.h"
#include "kprintf.h"

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