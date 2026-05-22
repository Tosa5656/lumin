#include "drivers/vga/vga.h"
#include "idt.h"

unsigned char keyboard_color;

void kmain(void)
{
    unsigned char color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    vga_init();
    vga_clear(vga_entry_color(VGA_WHITE, VGA_BLACK));
    vga_write("Hello World!", color);

    keyboard_color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    idt_init();

    __asm__("sti");

    while (1)
        __asm__("hlt");
}