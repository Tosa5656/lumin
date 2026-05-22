#include "drivers/vga/vga.h"
#include "idt.h"
#include "drivers/timer/timer.h"

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

void kmain(void)
{
    unsigned char color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    vga_init();
    vga_clear(vga_entry_color(VGA_WHITE, VGA_BLACK));
    vga_write("Hello World!", color);

    keyboard_color = vga_entry_color(VGA_WHITE, VGA_BLACK);
    idt_init();

    timer_init(1000);

    unsigned char green = vga_entry_color(VGA_GREEN, VGA_BLACK);
    vga_write("\nTimer: ", color);
    vga_write(timer_name(timer_get_type()), green);

    unsigned char cyan = vga_entry_color(VGA_CYAN, VGA_BLACK);
    int count = 0;

    __asm__("sti");

    while (1)
    {
        timer_sleep(1000);
        count++;

        unsigned char row_color = (count % 2) ? vga_entry_color(VGA_WHITE, VGA_BLACK) : vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_write("\nTick: ", row_color);

        char buf[32];
        int n = count;
        int i = 0;
        if (n == 0) buf[i++] = '0';
        while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
        for (int j = 0; j < i / 2; j++) { char tmp = buf[j]; buf[j] = buf[i - 1 - j]; buf[i - 1 - j] = tmp; }
        buf[i] = '\0';
        vga_write(buf, row_color);
    }
}