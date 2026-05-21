#include "idt.h"
#include "ports.h"

#define IDT_SIZE 256

static struct idt_entry_t idt[IDT_SIZE] __attribute__((aligned(16)));

extern void isr32_handler(void);
extern void isr33_handler(void);
extern void keyboard_handler(void);

static void pic_remap(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

static void idt_set_entry(int index, void *handler, uint16_t selector, uint8_t attributes)
{
    uint64_t addr = (uint64_t)handler;
    idt[index].offset_low   = addr & 0xFFFF;
    idt[index].offset_mid   = (addr >> 16) & 0xFFFF;
    idt[index].offset_high  = (addr >> 32) & 0xFFFFFFFF;
    idt[index].selector     = selector;
    idt[index].ist          = 0;
    idt[index].attributes   = attributes;
    idt[index].reserved     = 0;
}

void irq_handler(uint64_t int_no)
{
    if (int_no == 33)
        keyboard_handler();
    outb(0x20, 0x20);
}

void idt_init(void)
{
    pic_remap();

    idt_set_entry(32, isr32_handler, 0x08, 0x8E);
    idt_set_entry(33, isr33_handler, 0x08, 0x8E);

    struct idt_ptr_t idt_ptr;
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
