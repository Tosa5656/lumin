#include "panic.h"

static void write_centered(volatile unsigned short *buf, int row, const char *s, unsigned char color)
{
    int len = 0;
    while (s[len]) len++;
    int col = (VGA_WIDTH - len) / 2;
    if (col < 0) col = 0;
    for (int i = 0; s[i]; i++)
        buf[row * VGA_WIDTH + col + i] = vga_entry(s[i], color);
}

void panic(const char *title, const char *msg)
{
    __asm__ volatile("cli");

    unsigned char color = vga_entry_color(PANIC_FG, PANIC_BG);
    volatile unsigned short *buf = (volatile unsigned short *)VGA_ADDRESS;

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        buf[i] = vga_entry(' ', color);

    int row = 0;
    write_centered(buf, row++, "===== KERNEL PANIC =====", color);
    row++;
    write_centered(buf, row++, title, color);
    row++;
    write_centered(buf, row++, msg, color);

    for (;;)
        __asm__ volatile("hlt");
}