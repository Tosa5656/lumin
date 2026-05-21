#include "drivers/vga/vga.h"

void kmain(void)
{
    unsigned char color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    vga_init();
    vga_clear(vga_entry_color(VGA_WHITE, VGA_CYAN));
    vga_write("Hello World!", color);
}