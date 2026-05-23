#include "vga.h"
#include <stddef.h>
#include "kprintf.h"

static int vga_row = 0;
static int vga_col = 0;

static volatile unsigned short* vga_buffer(void)
{
    return (volatile unsigned short*)VGA_ADDRESS;
}

static void vga_scroll(void)
{
    volatile unsigned short* buf = vga_buffer();
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            buf[(y - 1) * VGA_WIDTH + x] = buf[y * VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK));
    vga_row = VGA_HEIGHT - 1;
}

void vga_init(void)
{
    vga_row = 0;
    vga_col = 0;
}

void vga_clear(unsigned char color)
{
    volatile unsigned short* buf = vga_buffer();
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        buf[i] = vga_entry(' ', color);
    vga_row = 0;
    vga_col = 0;
}

void vga_putchar(char c, unsigned char color)
{
    volatile unsigned short* buf = vga_buffer();
    if (c == '\n')
    {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT)
            vga_scroll();
        return;
    }
    if (c == '\t')
    {
        int stop = (vga_col + 4) & ~3;
        while (vga_col < stop && vga_col < VGA_WIDTH)
            buf[vga_row * VGA_WIDTH + vga_col++] = vga_entry(' ', color);
        if (vga_col == VGA_WIDTH)
        {
            vga_col = 0;
            if (++vga_row == VGA_HEIGHT)
                vga_scroll();
        }
        return;
    }
    buf[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, color);
    if (++vga_col == VGA_WIDTH)
    {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT)
            vga_scroll();
    }
}

void vga_write(const char* str, unsigned char color)
{
    for (int i = 0; str[i] != '\0'; i++)
        vga_putchar(str[i], color);
}

void vga_backspace(void)
{
    if (vga_col > 0)
    {
        vga_col--;
        volatile unsigned short* buf = vga_buffer();
        buf[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_default_color);
    }
}

void vga_set_cursor(int row, int col)
{
    if (row >= 0 && row < VGA_HEIGHT && col >= 0 && col < VGA_WIDTH)
    {
        vga_row = row;
        vga_col = col;
    }
}

unsigned char vga_default_color;

static void vga_putch(char c, void *ctx)
{
    (void)ctx;
    vga_putchar(c, vga_default_color);
}

void vga_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvformat(vga_putch, NULL, fmt, ap);
    va_end(ap);
}